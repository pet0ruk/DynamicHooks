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
// Hooking a function whose first instructions (the ones DynamicHooks copies into its trampoline)
// call a GCC -fPIC thunk ("call __x86.get_pc_thunk.<reg>": mov <reg>, [esp]; ret). Run from the
// trampoline, the call would return the trampoline's address, so DynamicHooks replaces it with
// "mov <reg>, <address after the original call>". Each function here returns the register the
// thunk set, which must be that address, for every register GCC uses for thunks.
// ============================================================================
#include "assert.h"

#include "manager.h"
#include "conventions/x86GccCdecl.h"

// int pic_thunk_<reg>(): push ebp; mov ebp, esp; push <reg>; call thunk_<reg>;
// pic_thunk_<reg>_pc: xchg <reg>, [esp]; pop eax; pop ebp; ret (returns the thunk's value and
// restores <reg>, also for eax)
#define PIC_THUNK_FUNCTION(reg) \
	".globl pic_thunk_" #reg "\n" \
	".type pic_thunk_" #reg ", @function\n" \
	"pic_thunk_" #reg ":\n" \
	"	push %ebp\n" \
	"	mov %esp, %ebp\n" \
	"	push %" #reg "\n" \
	"	call thunk_" #reg "\n" \
	".globl pic_thunk_" #reg "_pc\n" \
	"pic_thunk_" #reg "_pc:\n" \
	"	xchg %" #reg ", (%esp)\n" \
	"	pop %eax\n" \
	"	pop %ebp\n" \
	"	ret\n" \
	"thunk_" #reg ":\n" \
	"	mov (%esp), %" #reg "\n" \
	"	ret\n"

__asm__(".text\n"
	PIC_THUNK_FUNCTION(eax) PIC_THUNK_FUNCTION(ebx) PIC_THUNK_FUNCTION(ecx)
	PIC_THUNK_FUNCTION(edx) PIC_THUNK_FUNCTION(esi) PIC_THUNK_FUNCTION(edi)
	PIC_THUNK_FUNCTION(ebp));

#define DECLARE_PIC_THUNK(reg) \
	extern "C" int pic_thunk_##reg(); \
	extern "C" char pic_thunk_##reg##_pc[];
DECLARE_PIC_THUNK(eax) DECLARE_PIC_THUNK(ebx) DECLARE_PIC_THUNK(ecx) DECLARE_PIC_THUNK(edx)
DECLARE_PIC_THUNK(esi) DECLARE_PIC_THUNK(edi) DECLARE_PIC_THUNK(ebp)

int g_iPreCallCount = 0;

bool PreFunc(HookType_t eHookType, CHook* pHook)
{
	g_iPreCallCount++;
	return false;
}

static void Test(int (*pFunc)(), char* pExpected)
{
	assert(pFunc() == (int) pExpected);

	CHookManager* pHookMngr = GetHookManager();
	std::vector<DataType_t> vecArgTypes;
	CHook* pHook = pHookMngr->HookFunction((void *) pFunc, new x86GccCdecl(vecArgTypes, DATA_TYPE_INT));
	pHook->AddCallback(HOOKTYPE_PRE, (HookHandlerFn *) (void *) &PreFunc);

	int iBefore = g_iPreCallCount;
	assert(pFunc() == (int) pExpected);
	assert(g_iPreCallCount == iBefore + 1);

	pHookMngr->UnhookFunction((void *) pFunc);
	assert(pFunc() == (int) pExpected);
}

int main()
{
	Test(&pic_thunk_eax, pic_thunk_eax_pc);
	Test(&pic_thunk_ebx, pic_thunk_ebx_pc);
	Test(&pic_thunk_ecx, pic_thunk_ecx_pc);
	Test(&pic_thunk_edx, pic_thunk_edx_pc);
	Test(&pic_thunk_esi, pic_thunk_esi_pc);
	Test(&pic_thunk_edi, pic_thunk_edi_pc);
	Test(&pic_thunk_ebp, pic_thunk_ebp_pc);
	return 0;
}
