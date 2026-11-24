/**
* =============================================================================
* DynamicHooks
* Copyright (C) 2015 Robin Gohmert. All rights reserved.
* =============================================================================
*
* This software is provided 'as-is', without any express or implied warranty.
* In no event will the authors be held liable for any damages arising from
* the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software in a
* product, an acknowledgment in the product documentation would be
* appreciated but is not required.
*
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
*
* 3. This notice may not be removed or altered from any source distribution.
*/

// ============================================================================
// Hooking functions whose first instructions need more than a plain copy into the trampoline,
// and functions that can't be hooked. The functions are machine code written into executable
// memory, so each has exactly the instructions it is about, at an exact place. Every function
// is cdecl int f(int x).
// ============================================================================
#include "assert.h"
#include <stdio.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <sys/mman.h>
	#include <unistd.h>
#endif

#include "manager.h"
#include "conventions/x86MsCdecl.h"


// ============================================================================
// >> Executable memory
// ============================================================================
typedef int (*Func)(int);
typedef std::vector<unsigned char> Code;

static size_t PageSize()
{
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
#else
	return (size_t) sysconf(_SC_PAGESIZE);
#endif
}

/*
Returns two pages of read-only executable memory with <code> at <offset> (padded with int3, as
compilers pad between functions).
*/
static unsigned char* MakeCode(const Code& code, size_t offset = 0)
{
	size_t size = 2 * PageSize();
#ifdef _WIN32
	unsigned char* pages = (unsigned char *) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	assert(pages);
#else
	unsigned char* pages = (unsigned char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(pages != MAP_FAILED);
#endif
	memset(pages, 0xCC, size);
	memcpy(pages + offset, code.data(), code.size());
#ifdef _WIN32
	DWORD old;
	assert(VirtualProtect(pages, size, PAGE_EXECUTE_READ, &old));
#else
	assert(mprotect(pages, size, PROT_READ | PROT_EXEC) == 0);
#endif
	return pages + offset;
}

static bool IsWritable(const void* address)
{
#ifdef _WIN32
	MEMORY_BASIC_INFORMATION info;
	assert(VirtualQuery(address, &info, sizeof(info)));
	return (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
#else
	FILE* file = fopen("/proc/self/maps", "r");
	assert(file);
	char line[512];
	bool writable = false, found = false;
	while (!found && fgets(line, sizeof(line), file))
	{
		unsigned long start, end;
		char perms[5];
		if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3
			&& (unsigned long) address >= start && (unsigned long) address < end)
		{
			found = true;
			writable = perms[1] == 'w';
		}
	}
	fclose(file);
	assert(found);
	return writable;
#endif
}

// Appends the little-endian 32-bit offset from <from> (the end of the instruction) to <to>.
static void Rel32(Code& code, int from, int to)
{
	int offset = to - from;
	unsigned char bytes[4];
	memcpy(bytes, &offset, 4);
	code.insert(code.end(), bytes, bytes + 4);
}


// ============================================================================
// >> Hook callbacks
// ============================================================================
int g_iPreCalls = 0;
int g_iPostCalls = 0;
int g_iArgument = 0;
int g_iReturnValue = 0;

bool PreHook(HookType_t eHookType, CHook* pHook)
{
	g_iPreCalls++;
	g_iArgument = pHook->GetArgument<int>(0);
	return false;
}

bool PostHook(HookType_t eHookType, CHook* pHook)
{
	g_iPostCalls++;
	g_iReturnValue = pHook->GetReturnValue<int>();
	pHook->SetReturnValue<int>(g_iReturnValue * 10);
	return false;
}

static ICallingConvention* MakeConvention()
{
	std::vector<DataType_t> vecArgTypes;
	vecArgTypes.push_back(DATA_TYPE_INT);
	return new x86MsCdecl(vecArgTypes, DATA_TYPE_INT);
}


// ============================================================================
// >> Tests
// ============================================================================
/*
Hooks <code> (placed at <offset> in its pages), which must return <expected> for 5, checks the
hook sees and changes the call, then unhooks it and checks the function is restored.
*/
static void TestHook(const char* name, const Code& code, int expected, size_t offset = 0)
{
	printf("%s\n", name);
	unsigned char* pFunc = MakeCode(code, offset);
	Func func = (Func) pFunc;
	assert(func(5) == expected);

	CHook* pHook = GetHookManager()->HookFunction(pFunc, MakeConvention());
	assert(pHook);
	pHook->AddCallback(HOOKTYPE_PRE, (HookHandlerFn *) (void *) &PreHook);
	pHook->AddCallback(HOOKTYPE_POST, (HookHandlerFn *) (void *) &PostHook);

	// Patching must leave every page of the function as it was (not writable)
	assert(!IsWritable(pFunc));
	assert(!IsWritable(pFunc + code.size() - 1));

	g_iPreCalls = g_iPostCalls = 0;
	assert(func(5) == expected * 10);
	assert(g_iPreCalls == 1);
	assert(g_iPostCalls == 1);
	assert(g_iArgument == 5);
	assert(g_iReturnValue == expected);

	// The original function, called through the trampoline
	Func original = (Func) pHook->m_pTrampoline;
	assert(original(5) == expected);
	assert(g_iPreCalls == 1);

	GetHookManager()->UnhookFunction(pFunc);
	assert(memcmp(pFunc, code.data(), code.size()) == 0);
	assert(!IsWritable(pFunc));
	assert(!IsWritable(pFunc + code.size() - 1));
	assert(func(5) == expected);
}

/*
Checks <code> (which must return <expected> for 5) can't be hooked, and that trying leaves it
unchanged.
*/
static void TestRefused(const char* name, const Code& code, int expected)
{
	printf("%s\n", name);
	unsigned char* pFunc = MakeCode(code);
	ICallingConvention* pConvention = MakeConvention();
	assert(GetHookManager()->HookFunction(pFunc, pConvention) == NULL);
	assert(GetHookManager()->FindHook(pFunc) == NULL);

	// The convention still belongs to the caller
	assert(!pConvention->m_bHooked);
	delete pConvention;

	assert(memcmp(pFunc, code.data(), code.size()) == 0);
	assert(!IsWritable(pFunc));
	if (expected)
		assert(((Func) pFunc)(5) == expected);
}

// mov eax, [esp+4]
#define LOAD_X 0x8B, 0x44, 0x24, 0x04
// mov eax, 99 (never run)
#define MOV_99 0xB8, 0x63, 0x00, 0x00, 0x00

int main()
{
	// The hook's 5-byte jump spans two pages: the function starts 2 bytes before a page end.
	{
		unsigned char bytes[] = {LOAD_X, 0x83, 0xC0, 0x01, 0xC3};   // add eax, 1; ret
		TestHook("page boundary", Code(bytes, bytes + sizeof(bytes)), 6, PageSize() - 2);
	}

	// jmp rel8 among the copied instructions, to beyond them: widened to jmp rel32.
	{
		unsigned char bytes[] = {
			LOAD_X,
			0xEB, 0x06,				// 4: jmp 12
			MOV_99, 0x90,			// 6
			0x83, 0xC0, 0x02,		// 12: add eax, 2
			0xC3};
		TestHook("jmp rel8", Code(bytes, bytes + sizeof(bytes)), 7);
	}

	// jcc rel8, taken and not taken: widened to jcc rel32.
	{
		unsigned char bytes[] = {
			0x31, 0xC0,				// 0: xor eax, eax (ZF = 1)
			0x74, 0x08,				// 2: jz 12
			MOV_99,					// 4
			0xC3, 0x90, 0x90,		// 9
			LOAD_X,					// 12
			0x83, 0xC0, 0x03,		// add eax, 3
			0xC3};
		TestHook("jcc rel8 (taken)", Code(bytes, bytes + sizeof(bytes)), 8);
	}
	{
		unsigned char bytes[] = {
			0x31, 0xC0,				// 0: xor eax, eax (ZF = 1)
			0x75, 0x08,				// 2: jnz 12
			LOAD_X,					// 4
			0xC3, 0x90, 0x90, 0x90,	// 8
			MOV_99,					// 12
			0xC3};
		TestHook("jcc rel8 (not taken)", Code(bytes, bytes + sizeof(bytes)), 5);
	}

	// jcc rel32 among the copied instructions.
	{
		unsigned char bytes[] = {
			0x31, 0xC0,							// 0: xor eax, eax
			0x0F, 0x84, 0x06, 0x00, 0x00, 0x00,	// 2: jz 14
			MOV_99,								// 8
			0xC3,								// 13
			LOAD_X,								// 14
			0x83, 0xC0, 0x04,					// add eax, 4
			0xC3};
		TestHook("jcc rel32", Code(bytes, bytes + sizeof(bytes)), 9);
	}

	// call rel32 as the first instruction: the callee returns into the trampoline.
	{
		Code code;
		code.push_back(0xE8); Rel32(code, 5, 16);	// 0: call 16
		unsigned char rest[] = {
			0x83, 0xC0, 0x05,					// 5: add eax, 5
			0xC3,								// 8
			0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
			0x8B, 0x44, 0x24, 0x08,				// 16: mov eax, [esp+8]
			0xC3};
		code.insert(code.end(), rest, rest + sizeof(rest));
		TestHook("call rel32", code, 10);
	}

	// jmp rel32 as the whole first 5 bytes (e.g. an incremental-linking thunk).
	{
		Code code;
		code.push_back(0xE9); Rel32(code, 5, 16);	// 0: jmp 16
		unsigned char rest[] = {
			0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
			LOAD_X,								// 16
			0x83, 0xC0, 0x06,					// add eax, 6
			0xC3};
		code.insert(code.end(), rest, rest + sizeof(rest));
		TestHook("jmp rel32", code, 11);
	}

	// Functions shorter than the hook's jump.
	{
		unsigned char bytes[] = {0x31, 0xC0, 0xC3};		// xor eax, eax; ret (then int3 padding)
		TestRefused("short function (ret)", Code(bytes, bytes + sizeof(bytes)), 0);
	}
	{
		unsigned char bytes[] = {0x31, 0xC0, 0xC3, LOAD_X, 0xC3};	// ret, then another function
		TestRefused("short function (ret, no padding)", Code(bytes, bytes + sizeof(bytes)), 0);
	}
	{
		unsigned char bytes[] = {0x40, 0x40};	// inc eax; inc eax; then int3 padding
		TestRefused("int3 padding", Code(bytes, bytes + sizeof(bytes)), 0);
	}

	// A jmp rel8 that leaves the function before 5 bytes.
	{
		unsigned char bytes[] = {0xEB, 0x06, MOV_99, 0x90, LOAD_X, 0xC3};
		TestRefused("jmp rel8 first", Code(bytes, bytes + sizeof(bytes)), 5);
	}

	// A branch into the bytes the hook's jump replaces.
	{
		unsigned char bytes[] = {
			0x31, 0xC0,			// 0: xor eax, eax
			0x74, 0x00,			// 2: jz 4
			0x90,				// 4
			LOAD_X,				// 5
			0xC3};
		TestRefused("branch into replaced bytes", Code(bytes, bytes + sizeof(bytes)), 5);
	}

	// jecxz has no 32-bit form.
	{
		unsigned char bytes[] = {
			0x31, 0xC9,			// 0: xor ecx, ecx
			0xE3, 0x01,			// 2: jecxz 5
			0x90,				// 4
			LOAD_X,				// 5
			0xC3};
		TestRefused("jecxz", Code(bytes, bytes + sizeof(bytes)), 5);
	}

	GetHookManager()->UnhookAllFunctions();
	return 0;
}
