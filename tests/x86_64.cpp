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
// x86-64 hooks (tests/CMakeLists.txt builds this once per return type):
//   DH_TEST_RETURN_INT / _LONG_LONG / _FLOAT / _DOUBLE / _POINTER
// with the platform's convention (System V on Linux, Microsoft's on Windows).
//
// 1. A function of 16 arguments of every kind, so that on both conventions some are in
//    general purpose registers, some in XMM registers and some on the stack: the pre-hook checks
//    and changes them (the function must see the new values), the post-hook checks and replaces
//    the return value (the caller must see it); a pre-hook that returns true skips the function;
//    unhooked, it runs unchanged and its bytes are restored.
// 2. A member function (this is the first argument).
// 3. A recursive function: every call's return address is kept.
// 4. Handlers run with a 16-byte aligned stack.
// 5. Machine code whose first instruction (in the bytes the hook's jump replaces) is
//    RIP-relative: the trampoline must still read the right address.
// 6. A function shorter than the hook's jump, followed by int3 padding up to the next 16-byte
//    boundary (as MSVC aligns functions), is hooked. Other functions shorter than the jump
//    (followed by other code, or by int3 padding the jump would reach past) are refused, and
//    left unchanged.
// ============================================================================
#include "assert.h"
#include "string.h"
#include "stdint.h"
#include "manager.h"

#ifdef _WIN32
	#include <windows.h>
	#include "conventions/x86_64MicrosoftDefault.h"
	typedef x86_64MicrosoftDefault Convention;
	#define NOINLINE __declspec(noinline)
#else
	#include <sys/mman.h>
	#include "conventions/x86_64SystemVDefault.h"
	typedef x86_64SystemVDefault Convention;
	#define NOINLINE __attribute__((noinline))
#endif

static std::shared_ptr<ICallingConvention> MakeConvention(std::vector<DataType_t> args, DataType_t ret)
{
	return std::make_shared<Convention>(std::move(args), ret);
}


// ============================================================================
// >> Return values
// ============================================================================
static int g_Dummy[3];
#if defined(DH_TEST_RETURN_INT)
	typedef int ret_t;
	static const DataType_t RETURN_TYPE = DATA_TYPE_INT;
	#define RET_ORIGINAL 9
	#define RET_PRE -42
	#define RET_POST 1337
#elif defined(DH_TEST_RETURN_LONG_LONG)
	typedef long long ret_t;
	static const DataType_t RETURN_TYPE = DATA_TYPE_LONG_LONG;
	#define RET_ORIGINAL 0x100000009LL
	#define RET_PRE -0x2A00000001LL
	#define RET_POST 0x7000000000000539LL
#elif defined(DH_TEST_RETURN_FLOAT)
	typedef float ret_t;
	static const DataType_t RETURN_TYPE = DATA_TYPE_FLOAT;
	#define RET_ORIGINAL 9.5f
	#define RET_PRE -42.75f
	#define RET_POST 1337.25f
#elif defined(DH_TEST_RETURN_DOUBLE)
	typedef double ret_t;
	static const DataType_t RETURN_TYPE = DATA_TYPE_DOUBLE;
	#define RET_ORIGINAL 9.5
	#define RET_PRE -42.125
	#define RET_POST 1337.25
#elif defined(DH_TEST_RETURN_POINTER)
	typedef int* ret_t;
	static const DataType_t RETURN_TYPE = DATA_TYPE_POINTER;
	#define RET_ORIGINAL (&g_Dummy[0])
	#define RET_PRE (&g_Dummy[1])
	#define RET_POST (&g_Dummy[2])
#endif


// ============================================================================
// >> Helpers
// ============================================================================
static bool IsStackAligned()
{
	alignas(16) volatile char probe[16];
	probe[0] = 0;
	return (reinterpret_cast<uintptr_t>(&probe[0]) & 15) == 0;
}

static int g_iPre = 0, g_iPost = 0, g_iCalls = 0;


// ============================================================================
// >> 1. Many arguments
// ============================================================================
// System V: a..f, ptr in rdi, rsi, rdx, rcx, r8, r9; i, j on the stack; the 8 doubles/floats
// b..p in xmm0-7 except the last ones on the stack. Microsoft: a, b, c, d by position
// (rcx, xmm1, r8, xmm3), everything else on the stack.
static const std::vector<DataType_t> MANY_ARGS = {
	DATA_TYPE_INT, DATA_TYPE_DOUBLE, DATA_TYPE_LONG_LONG, DATA_TYPE_FLOAT,
	DATA_TYPE_SHORT, DATA_TYPE_POINTER, DATA_TYPE_INT, DATA_TYPE_DOUBLE,
	DATA_TYPE_CHAR, DATA_TYPE_LONG_LONG, DATA_TYPE_DOUBLE, DATA_TYPE_FLOAT,
	DATA_TYPE_DOUBLE, DATA_TYPE_DOUBLE, DATA_TYPE_DOUBLE, DATA_TYPE_DOUBLE,
};

static bool g_bChanged = false;
static int g_Target = 0;

NOINLINE ret_t Many(int a, double b, long long c, float d, short e, int* f, int g, double h,
	char i, long long j, double k, float l, double m, double n, double o, double p)
{
	g_iCalls++;
	assert(IsStackAligned());
	assert(b == 1.5 && d == 2.5f && e == 5 && f == &g_Target && h == 7.25 && i == 'x');
	assert(k == 11.5 && l == 12.5f && m == 13.5 && n == 14.5 && o == 15.5);
	if (g_bChanged)
	{
		// changed by the pre-hook: one argument in a general purpose register, one in an XMM
		// register and some on the stack (on both conventions)
		assert(a == -1 && c == -3 && g == -7 && j == -10 && p == -16.5);
	}
	else
	{
		assert(a == 1 && c == 0x300000003LL && g == 7 && j == 0x10000000AALL && p == 16.5);
	}
	return RET_ORIGINAL;
}

static void CheckOriginalArguments(CHook* pHook)
{
	assert(pHook->GetArgument<int>(0) == 1);
	assert(pHook->GetArgument<double>(1) == 1.5);
	assert(pHook->GetArgument<long long>(2) == 0x300000003LL);
	assert(pHook->GetArgument<float>(3) == 2.5f);
	assert(pHook->GetArgument<short>(4) == 5);
	assert(pHook->GetArgument<int*>(5) == &g_Target);
	assert(pHook->GetArgument<int>(6) == 7);
	assert(pHook->GetArgument<double>(7) == 7.25);
	assert(pHook->GetArgument<char>(8) == 'x');
	assert(pHook->GetArgument<long long>(9) == 0x10000000AALL);
	assert(pHook->GetArgument<double>(10) == 11.5);
	assert(pHook->GetArgument<float>(11) == 12.5f);
	assert(pHook->GetArgument<double>(12) == 13.5);
	assert(pHook->GetArgument<double>(13) == 14.5);
	assert(pHook->GetArgument<double>(14) == 15.5);
	assert(pHook->GetArgument<double>(15) == 16.5);
}

static bool PreMany(HookType_t, CHook* pHook)
{
	g_iPre++;
	assert(IsStackAligned());
	CheckOriginalArguments(pHook);
	pHook->SetArgument<int>(0, -1);
	pHook->SetArgument<long long>(2, -3);
	pHook->SetArgument<int>(6, -7);
	pHook->SetArgument<long long>(9, -10);
	pHook->SetArgument<double>(15, -16.5);
	return false;
}

static bool PreManyOverride(HookType_t, CHook* pHook)
{
	g_iPre++;
	CheckOriginalArguments(pHook);
	pHook->SetReturnValue<ret_t>(RET_PRE);
	return true;
}

static ret_t g_PostExpects;
static bool PostMany(HookType_t, CHook* pHook)
{
	g_iPost++;
	assert(IsStackAligned());
	assert(pHook->GetReturnValue<ret_t>() == g_PostExpects);
	pHook->SetReturnValue<ret_t>(RET_POST);
	return false;
}

// Through a volatile pointer, so the compiler can't inline or skip the call.
static ret_t (*volatile g_pMany)(int, double, long long, float, short, int*, int, double, char,
	long long, double, float, double, double, double, double) = &Many;

static ret_t CallMany()
{
	return g_pMany(1, 1.5, 0x300000003LL, 2.5f, 5, &g_Target, 7, 7.25, 'x', 0x10000000AALL,
		11.5, 12.5f, 13.5, 14.5, 15.5, 16.5);
}

static void TestManyArguments(CHookManager* pManager)
{
	void* pFunc = reinterpret_cast<void*>(&Many);
	unsigned char original[32];
	memcpy(original, pFunc, sizeof(original));

	CHook* pHook = pManager->HookFunction(pFunc, MakeConvention(MANY_ARGS, RETURN_TYPE));
	assert(pHook);
	pHook->AddCallback(HOOKTYPE_PRE, &PreMany);
	pHook->AddCallback(HOOKTYPE_POST, &PostMany);

	// 1. changed arguments and return value
	g_bChanged = true;
	g_PostExpects = RET_ORIGINAL;
	assert(CallMany() == RET_POST);
	assert(g_iCalls == 1 && g_iPre == 1 && g_iPost == 1);

	// 2. skipped by the pre-hook
	pHook->RemoveCallback(HOOKTYPE_PRE, &PreMany);
	pHook->AddCallback(HOOKTYPE_PRE, &PreManyOverride);
	g_PostExpects = RET_PRE;
	assert(CallMany() == RET_POST);
	assert(g_iCalls == 1 && g_iPre == 2 && g_iPost == 2);

	// The trampoline calls the function without the hook.
	g_bChanged = false;
	ret_t (*pTrampoline)(int, double, long long, float, short, int*, int, double, char,
		long long, double, float, double, double, double, double);
	pTrampoline = reinterpret_cast<decltype(pTrampoline)>(pHook->GetTrampoline());
	assert(pTrampoline(1, 1.5, 0x300000003LL, 2.5f, 5, &g_Target, 7, 7.25, 'x', 0x10000000AALL,
		11.5, 12.5f, 13.5, 14.5, 15.5, 16.5) == RET_ORIGINAL);
	assert(g_iCalls == 2 && g_iPre == 2 && g_iPost == 2);

	// 3. unhooked
	pManager->UnhookFunction(pFunc);
	assert(memcmp(original, pFunc, sizeof(original)) == 0);
	assert(CallMany() == RET_ORIGINAL);
	assert(g_iCalls == 3 && g_iPre == 2 && g_iPost == 2);
}


// ============================================================================
// >> 2. A member function
// ============================================================================
class MyClass
{
public:
	NOINLINE ret_t Method(int z, double y)
	{
		g_iCalls++;
		assert(this == s_pInstance && y == 2.25);
		return z == 30 ? RET_ORIGINAL : RET_PRE;
	}
	static MyClass* s_pInstance;
};
MyClass* MyClass::s_pInstance = nullptr;

static bool PreMethod(HookType_t, CHook* pHook)
{
	g_iPre++;
	assert(pHook->GetArgument<MyClass*>(0) == MyClass::s_pInstance);
	assert(pHook->GetArgument<int>(1) == 3 && pHook->GetArgument<double>(2) == 2.25);
	pHook->SetArgument<int>(1, 30);
	return false;
}

static void TestMemberFunction(CHookManager* pManager)
{
	MyClass instance;
	MyClass::s_pInstance = &instance;
	ret_t (MyClass::*volatile pMethod)(int, double) = &MyClass::Method;

	// The address of the (non-virtual) member function.
	void* pFunc;
	auto method = &MyClass::Method;
	memcpy(&pFunc, &method, sizeof(pFunc));

	CHook* pHook = pManager->HookFunction(pFunc,
		MakeConvention({DATA_TYPE_POINTER, DATA_TYPE_INT, DATA_TYPE_DOUBLE}, RETURN_TYPE));
	assert(pHook);
	pHook->AddCallback(HOOKTYPE_PRE, &PreMethod);
	g_iCalls = g_iPre = 0;
	assert((instance.*pMethod)(3, 2.25) == RET_ORIGINAL);
	assert(g_iCalls == 1 && g_iPre == 1);
	pManager->UnhookFunction(pFunc);
	assert((instance.*pMethod)(3, 2.25) == RET_PRE);
}


// ============================================================================
// >> 3. Recursion
// ============================================================================
static long long (*volatile g_pFactorial)(long long);

NOINLINE long long Factorial(long long n)
{
	return n <= 1 ? 1 : n * g_pFactorial(n - 1);
}

static int g_iFactorialPost = 0;
static bool PostFactorial(HookType_t, CHook* pHook)
{
	// each call returns to its own caller with its own value
	g_iFactorialPost++;
	pHook->SetUsePreRegisters(false);
	return false;
}

static void TestRecursion(CHookManager* pManager)
{
	g_pFactorial = &Factorial;
	void* pFunc = reinterpret_cast<void*>(&Factorial);
	CHook* pHook = pManager->HookFunction(pFunc, MakeConvention({DATA_TYPE_LONG_LONG}, DATA_TYPE_LONG_LONG));
	assert(pHook);
	pHook->AddCallback(HOOKTYPE_POST, &PostFactorial);
	assert(g_pFactorial(10) == 3628800LL);
	assert(g_iFactorialPost == 10);
	pManager->UnhookFunction(pFunc);
}


// ============================================================================
// >> 5. A RIP-relative first instruction
// ============================================================================
// mov rax, [rip + disp32]; add rax, <first argument>; ret; then int3 padding; the 64-bit value
// it loads is at VALUE_OFFSET in the same page.
static const size_t VALUE_OFFSET = 64;

static void* AllocateCode(size_t size)
{
#ifdef _WIN32
	return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
	void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return p == MAP_FAILED ? nullptr : p;
#endif
}

static bool PreRipRelative(HookType_t, CHook* pHook)
{
	g_iPre++;
	assert(pHook->GetArgument<long long>(0) == 5);
	pHook->SetArgument<long long>(0, 6);
	return false;
}

static void TestRipRelative(CHookManager* pManager)
{
	unsigned char* pCode = static_cast<unsigned char*>(AllocateCode(4096));
	assert(pCode);
	memset(pCode, 0xCC, 4096);
	const unsigned char code[] = {
		0x48, 0x8B, 0x05, 0, 0, 0, 0,	// mov rax, [rip + disp32]
#ifdef _WIN32
		0x48, 0x01, 0xC8,				// add rax, rcx
#else
		0x48, 0x01, 0xF8,				// add rax, rdi
#endif
		0xC3,							// ret
	};
	memcpy(pCode, code, sizeof(code));
	int32_t disp = static_cast<int32_t>(VALUE_OFFSET - 7);
	memcpy(pCode + 3, &disp, sizeof(disp));
	long long value = 1000;
	memcpy(pCode + VALUE_OFFSET, &value, sizeof(value));

	long long (*pFunc)(long long) = reinterpret_cast<long long (*)(long long)>(pCode);
	assert(pFunc(5) == 1005);

	CHook* pHook = pManager->HookFunction(pCode, MakeConvention({DATA_TYPE_LONG_LONG}, DATA_TYPE_LONG_LONG));
	assert(pHook);
	pHook->AddCallback(HOOKTYPE_PRE, &PreRipRelative);
	g_iPre = 0;
	assert(pFunc(5) == 1006);
	assert(g_iPre == 1);

	// the trampoline's copy of the instruction still reads the value next to the function
	value = 2000;
	memcpy(pCode + VALUE_OFFSET, &value, sizeof(value));
	assert(pFunc(5) == 2006);

	pManager->UnhookFunction(pCode);
	assert(memcmp(pCode, code, 3) == 0 && pFunc(5) == 2005);
}


// ============================================================================
// >> 6. Functions the hook refuses
// ============================================================================
static bool PrePadding(HookType_t, CHook*)
{
	g_iPre++;
	return false;
}

static void TestRefused(CHookManager* pManager)
{
	// xor eax, eax; ret; then int3 padding (the rest of the page) to the 16-byte boundary
	{
		unsigned char* pCode = static_cast<unsigned char*>(AllocateCode(4096));
		assert(pCode);
		memset(pCode, 0xCC, 4096);
		const unsigned char code[] = { 0x31, 0xC0, 0xC3 };
		memcpy(pCode, code, sizeof(code));
		int (*pFunc)() = reinterpret_cast<int (*)()>(pCode);

		CHook* pHook = pManager->HookFunction(pCode, MakeConvention({}, DATA_TYPE_INT));
		assert(pHook);
		pHook->AddCallback(HOOKTYPE_PRE, &PrePadding);
		g_iPre = 0;
		assert(pFunc() == 0 && g_iPre == 1);
		pManager->UnhookFunction(pCode);
		assert(memcmp(pCode, code, sizeof(code)) == 0 && pCode[3] == 0xCC && pFunc() == 0);
	}

	struct Refused
	{
		size_t offset;				// in the page
		unsigned char code[16];
	};
	const Refused functions[] = {
		{ 0, { 0x31, 0xC0, 0xC3, 0x31, 0xC0, 0xFF, 0xC0, 0xC3 } },		// xor eax, eax; ret; (next function)
		{ 0, { 0x31, 0xC0, 0xC3, 0xCC, 0x90, 0x90, 0x90, 0x90 } },		// ...; ret; int3; (code)
		{ 12, { 0x31, 0xC0, 0xC3, 0xCC, 0x31, 0xC0, 0xC3, 0xCC } },	// ...; ret; int3 up to the boundary, then a function
		{ 0, { 0xEB, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3 } },	// jmp +0; nop...
		{ 0, { 0x90, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC } },		// nop; int3...
	};
	for (const Refused& function : functions)
	{
		unsigned char* pPage = static_cast<unsigned char*>(AllocateCode(4096));
		assert(pPage);
		memset(pPage, 0xCC, 4096);
		unsigned char* pCode = pPage + function.offset;
		memcpy(pCode, function.code, 16);

		assert(pManager->HookFunction(pCode, MakeConvention({}, DATA_TYPE_INT)) == nullptr);
		assert(memcmp(pCode, function.code, 16) == 0);
	}
}


// ============================================================================
// >> main
// ============================================================================
int main()
{
	CHookManager* pManager = GetHookManager();
	TestManyArguments(pManager);
	TestMemberFunction(pManager);
	TestRecursion(pManager);
	TestRipRelative(pManager);
	TestRefused(pManager);
	pManager->UnhookAllFunctions();
	return 0;
}
