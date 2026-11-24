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
*
* asm.h/cpp from devmaster.net (thanks cybermind) edited by pRED* to handle gcc
* -fPIC thunks correctly
*
* Idea and trampoline code taken from DynDetours (thanks your-name-here).
*/

// ============================================================================
// One calling convention and return type per build (tests/CMakeLists.txt):
//   DH_TEST_CDECL / DH_TEST_THISCALL / DH_TEST_STDCALL (MSVC) / DH_TEST_FASTCALL
//   DH_TEST_RETURN_INT / DH_TEST_RETURN_LONG_LONG / DH_TEST_RETURN_FLOAT / DH_TEST_RETURN_DOUBLE
//
// The hooked function takes (int z, float x, short s, double y, long long w), which covers
// stack arguments of every size and, for fastcall, register arguments that are not the first
// two arguments (z in ecx, s in edx; x, y and w on the stack). Three phases:
//   1. pre-hook checks the arguments and changes z, s, y and w (the function must see the new
//      values); post-hook checks the return value and replaces it (the caller must see it)
//   2. a pre-hook that sets the return value and returns true: the function is skipped, the
//      post-hook sees the pre-hook's value and the caller gets the post-hook's
//   3. unhooked: the function runs with the original arguments, no callback runs
// After every call the stack pointer must be where it was, and the x87 stack empty.
// ============================================================================
#include "assert.h"

#include "manager.h"
#include "conventions/x86MsFastcall.h"
#ifdef _MSC_VER
	#include "conventions/x86MsCdecl.h"
	#include "conventions/x86MsStdcall.h"
	#include "conventions/x86MsThiscall.h"
	#define CDECL_CONVENTION x86MsCdecl
	#define THISCALL_CONVENTION x86MsThiscall
	#define CC_CDECL __cdecl
	#define CC_STDCALL __stdcall
	#define CC_FASTCALL __fastcall
	#define CC_THISCALL __thiscall
	#define GET_ESP(v) __asm mov v, esp
#else
	#include "conventions/x86GccCdecl.h"
	#include "conventions/x86GccThiscall.h"
	#define CDECL_CONVENTION x86GccCdecl
	#define THISCALL_CONVENTION x86GccThiscall
	#define CC_CDECL __attribute__((cdecl))
	#define CC_FASTCALL __attribute__((fastcall))
	#define CC_THISCALL
	#define GET_ESP(v) __asm__ volatile("movl %%esp, %0" : "=r"(v))
#endif


// ============================================================================
// >> Return values
// ============================================================================
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
#endif


// ============================================================================
// >> Helpers
// ============================================================================
// Number of x87 registers in use (0 = empty stack, as the ABI requires between calls).
static int X87Depth()
{
	unsigned char env[28];
#ifdef _MSC_VER
	__asm fnstenv env
	__asm fldenv env
#else
	__asm__ volatile("fnstenv %0\n\tfldenv %0" : "+m"(env));
#endif
	unsigned short tag = *(unsigned short *) (env + 8);
	int iDepth = 0;
	for (int i = 0; i < 8; i++)
	{
		if (((tag >> (2 * i)) & 3) != 3)
			iDepth++;
	}
	return iDepth;
}


// ============================================================================
// >> The hooked function
// ============================================================================
#if defined(DH_TEST_THISCALL)
	static const int FIRST = 1;		// argument 0 is this
#else
	static const int FIRST = 0;
#endif

int g_iMyFuncCallCount = 0;
int g_iPreMyFuncCallCount = 0;
int g_iPostMyFuncCallCount = 0;
bool g_bArgumentsChanged = false;	// whether the function should see the pre-hook's arguments
ret_t g_PostExpects = 0;			// the return value the post-hook should see

class MyClass;
MyClass* g_pMyClass = NULL;

#define MY_FUNC_BODY \
	g_iMyFuncCallCount++; \
	assert(x == 1.5f); \
	if (g_bArgumentsChanged) \
	{ \
		assert(z == 30); \
		assert(s == -7); \
		assert(y == 4.5); \
		assert(w == -5); \
	} \
	else \
	{ \
		assert(z == 3); \
		assert(s == 7); \
		assert(y == 2.25); \
		assert(w == 0x123456789ALL); \
	} \
	return RET_ORIGINAL;

#if defined(DH_TEST_THISCALL)
class MyClass
{
public:
	ret_t CC_THISCALL MyFunc(int z, float x, short s, double y, long long w)
	{
		assert(this == g_pMyClass);
		MY_FUNC_BODY
	}
};
#elif defined(DH_TEST_STDCALL)
ret_t CC_STDCALL MyFunc(int z, float x, short s, double y, long long w) { MY_FUNC_BODY }
#elif defined(DH_TEST_FASTCALL)
ret_t CC_FASTCALL MyFunc(int z, float x, short s, double y, long long w) { MY_FUNC_BODY }
#else
ret_t CC_CDECL MyFunc(int z, float x, short s, double y, long long w) { MY_FUNC_BODY }
#endif


// ============================================================================
// >> Callbacks
// ============================================================================
static void CheckOriginalArguments(CHook* pHook)
{
#if defined(DH_TEST_THISCALL)
	assert(pHook->GetArgument<MyClass *>(0) == g_pMyClass);
#endif
	assert(pHook->GetArgument<int>(FIRST) == 3);
	assert(pHook->GetArgument<float>(FIRST + 1) == 1.5f);
	assert(pHook->GetArgument<short>(FIRST + 2) == 7);
	assert(pHook->GetArgument<double>(FIRST + 3) == 2.25);
	assert(pHook->GetArgument<long long>(FIRST + 4) == 0x123456789ALL);
}

bool PreMyFunc(HookType_t eHookType, CHook* pHook)
{
	g_iPreMyFuncCallCount++;
	CheckOriginalArguments(pHook);
	pHook->SetArgument<int>(FIRST, 30);
	pHook->SetArgument<short>(FIRST + 2, -7);
	pHook->SetArgument<double>(FIRST + 3, 4.5);
	pHook->SetArgument<long long>(FIRST + 4, -5);
	return false;
}

bool PreMyFuncOverride(HookType_t eHookType, CHook* pHook)
{
	g_iPreMyFuncCallCount++;
	CheckOriginalArguments(pHook);
	pHook->SetReturnValue<ret_t>(RET_PRE);
	return true;
}

bool PostMyFunc(HookType_t eHookType, CHook* pHook)
{
	g_iPostMyFuncCallCount++;
	assert(pHook->GetReturnValue<ret_t>() == g_PostExpects);
	pHook->SetReturnValue<ret_t>(RET_POST);
	return false;
}


// ============================================================================
// >> main
// ============================================================================
static ret_t CallMyFunc()
{
	unsigned long ulEspBefore, ulEspAfter;
	ret_t result;

	GET_ESP(ulEspBefore);
#if defined(DH_TEST_THISCALL)
	result = g_pMyClass->MyFunc(3, 1.5f, 7, 2.25, 0x123456789ALL);
#else
	result = MyFunc(3, 1.5f, 7, 2.25, 0x123456789ALL);
#endif
	GET_ESP(ulEspAfter);

	assert(ulEspBefore == ulEspAfter);
	assert(X87Depth() == 0);
	return result;
}

int main()
{
	CHookManager* pHookMngr = GetHookManager();

#if defined(DH_TEST_THISCALL)
	MyClass instance;
	g_pMyClass = &instance;
#endif

	std::vector<DataType_t> vecArgTypes;
#if defined(DH_TEST_THISCALL)
	vecArgTypes.push_back(DATA_TYPE_POINTER);
#endif
	vecArgTypes.push_back(DATA_TYPE_INT);
	vecArgTypes.push_back(DATA_TYPE_FLOAT);
	vecArgTypes.push_back(DATA_TYPE_SHORT);
	vecArgTypes.push_back(DATA_TYPE_DOUBLE);
	vecArgTypes.push_back(DATA_TYPE_LONG_LONG);

#if defined(DH_TEST_THISCALL)
	ret_t (CC_THISCALL MyClass::*pMyFunc)(int, float, short, double, long long) = &MyClass::MyFunc;
	CHook* pHook = pHookMngr->HookFunction((void *&) pMyFunc,
		std::make_shared<THISCALL_CONVENTION>(vecArgTypes, RETURN_TYPE));
#elif defined(DH_TEST_STDCALL)
	CHook* pHook = pHookMngr->HookFunction((void *) &MyFunc,
		std::make_shared<x86MsStdcall>(vecArgTypes, RETURN_TYPE));
#elif defined(DH_TEST_FASTCALL)
	CHook* pHook = pHookMngr->HookFunction((void *) &MyFunc,
		std::make_shared<x86MsFastcall>(vecArgTypes, RETURN_TYPE));
#else
	CHook* pHook = pHookMngr->HookFunction((void *) &MyFunc,
		std::make_shared<CDECL_CONVENTION>(vecArgTypes, RETURN_TYPE));
#endif
	assert(X87Depth() == 0);

	// 1. Arguments and return value changed by the hooks
	pHook->AddCallback(HOOKTYPE_PRE, &PreMyFunc);
	pHook->AddCallback(HOOKTYPE_POST, &PostMyFunc);
	g_bArgumentsChanged = true;
	g_PostExpects = RET_ORIGINAL;
	for (int i = 1; i <= 3; i++)
	{
		assert(CallMyFunc() == RET_POST);
		assert(g_iMyFuncCallCount == i);
		assert(g_iPreMyFuncCallCount == i);
		assert(g_iPostMyFuncCallCount == i);
	}

	// 2. The function skipped by the pre-hook
	pHook->RemoveCallback(HOOKTYPE_PRE, &PreMyFunc);
	pHook->AddCallback(HOOKTYPE_PRE, &PreMyFuncOverride);
	g_PostExpects = RET_PRE;
	for (int i = 1; i <= 3; i++)
	{
		assert(CallMyFunc() == RET_POST);
		assert(g_iMyFuncCallCount == 3);
		assert(g_iPreMyFuncCallCount == 3 + i);
		assert(g_iPostMyFuncCallCount == 3 + i);
	}

	// 3. Unhooked: the original function, untouched
	pHookMngr->UnhookAllFunctions();
	g_bArgumentsChanged = false;
	assert(CallMyFunc() == RET_ORIGINAL);
	assert(g_iMyFuncCallCount == 4);
	assert(g_iPreMyFuncCallCount == 6);
	assert(g_iPostMyFuncCallCount == 6);
	return 0;
}
