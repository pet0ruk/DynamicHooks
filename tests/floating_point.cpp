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
// Floating-point arguments and return values (x87 st0), for one calling convention
// and return type per build (tests/CMakeLists.txt):
//   DH_TEST_RETURN      float or double
//   DH_TEST_CDECL / DH_TEST_STDCALL (MSVC) / DH_TEST_THISCALL
// The pre-hook checks an int, a float and a double argument and changes the double;
// the function must see the new value. The post-hook checks the return value and
// replaces it; the caller must see the new value.
// ============================================================================
#include "assert.h"

#include "manager.h"
#ifdef _WIN32
	#include "conventions/x86MsCdecl.h"
	#include "conventions/x86MsStdcall.h"
	#include "conventions/x86MsThiscall.h"
	#define CDECL_CONVENTION x86MsCdecl
	#define THISCALL_CONVENTION x86MsThiscall
	#define THISCALL __thiscall
#else
	#include "conventions/x86GccCdecl.h"
	#include "conventions/x86GccThiscall.h"
	#define CDECL_CONVENTION x86GccCdecl
	#define THISCALL_CONVENTION x86GccThiscall
	#define THISCALL
#endif

typedef DH_TEST_RETURN ret_t;
static const DataType_t RETURN_TYPE = sizeof(ret_t) == sizeof(float) ? DATA_TYPE_FLOAT : DATA_TYPE_DOUBLE;

#if defined(DH_TEST_THISCALL)
	static const int FIRST = 1;		// argument 0 is this
#else
	static const int FIRST = 0;
#endif

int g_iMyFuncCallCount = 0;
int g_iPreMyFuncCallCount = 0;
int g_iPostMyFuncCallCount = 0;

class MyClass;
MyClass* g_pMyClass = NULL;

#define MY_FUNC_BODY \
	g_iMyFuncCallCount++; \
	assert(z == 3); \
	assert(x == 1.5f); \
	assert(y == 4.5);	/* set by the pre-hook (passed: 2.25) */ \
	return (ret_t) (z + x + y);	/* 9.0 */

#if defined(DH_TEST_THISCALL)
class MyClass
{
public:
	ret_t THISCALL MyFunc(int z, float x, double y)
	{
		assert(this == g_pMyClass);
		MY_FUNC_BODY
	}
};
#elif defined(DH_TEST_STDCALL)
ret_t __stdcall MyFunc(int z, float x, double y) { MY_FUNC_BODY }
#else
ret_t __cdecl MyFunc(int z, float x, double y) { MY_FUNC_BODY }
#endif

bool PreMyFunc(HookType_t eHookType, CHook* pHook)
{
	g_iPreMyFuncCallCount++;
#if defined(DH_TEST_THISCALL)
	assert(pHook->GetArgument<MyClass *>(0) == g_pMyClass);
#endif
	assert(pHook->GetArgument<int>(FIRST) == 3);
	assert(pHook->GetArgument<float>(FIRST + 1) == 1.5f);
	assert(pHook->GetArgument<double>(FIRST + 2) == 2.25);
	pHook->SetArgument<double>(FIRST + 2, 4.5);
	return false;
}

bool PostMyFunc(HookType_t eHookType, CHook* pHook)
{
	g_iPostMyFuncCallCount++;
	assert(pHook->GetReturnValue<ret_t>() == (ret_t) 9.0);
	pHook->SetReturnValue<ret_t>((ret_t) 1337.25);
	return false;
}

int main()
{
	CHookManager* pHookMngr = GetHookManager();

	std::vector<DataType_t> vecArgTypes;
#if defined(DH_TEST_THISCALL)
	vecArgTypes.push_back(DATA_TYPE_POINTER);
#endif
	vecArgTypes.push_back(DATA_TYPE_INT);
	vecArgTypes.push_back(DATA_TYPE_FLOAT);
	vecArgTypes.push_back(DATA_TYPE_DOUBLE);

#if defined(DH_TEST_THISCALL)
	ret_t (THISCALL MyClass::*pMyFunc)(int, float, double) = &MyClass::MyFunc;
	CHook* pHook = pHookMngr->HookFunction((void *&) pMyFunc,
		new THISCALL_CONVENTION(vecArgTypes, RETURN_TYPE));
#elif defined(DH_TEST_STDCALL)
	CHook* pHook = pHookMngr->HookFunction((void *) &MyFunc,
		new x86MsStdcall(vecArgTypes, RETURN_TYPE));
#else
	CHook* pHook = pHookMngr->HookFunction((void *) &MyFunc,
		new CDECL_CONVENTION(vecArgTypes, RETURN_TYPE));
#endif
	pHook->AddCallback(HOOKTYPE_PRE, (HookHandlerFn *) (void *) &PreMyFunc);
	pHook->AddCallback(HOOKTYPE_POST, (HookHandlerFn *) (void *) &PostMyFunc);

#if defined(DH_TEST_THISCALL)
	MyClass instance;
	g_pMyClass = &instance;
	ret_t return_value = instance.MyFunc(3, 1.5f, 2.25);
#else
	ret_t return_value = MyFunc(3, 1.5f, 2.25);
#endif

	assert(g_iMyFuncCallCount == 1);
	assert(g_iPreMyFuncCallCount == 1);
	assert(g_iPostMyFuncCallCount == 1);
	assert(return_value == (ret_t) 1337.25);

	pHookMngr->UnhookAllFunctions();
	return 0;
}
