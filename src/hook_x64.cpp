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
// >> INCLUDES
// ============================================================================
#include "hook.h"

// The x86-64 hook: patching the function (with safetyhook) and the code it jumps to. The 32-bit
// one is in hook_x86.cpp, what both have in common in hook.cpp.
#ifdef DYNAMICHOOKS_X86_64

#include "hook_jit.h"

#include <stdint.h>
#include <stdio.h>

using namespace asmjit;
using namespace asmjit::x86;


// ============================================================================
// >> HELPERS
// ============================================================================
/*
How the hook's code works (see CreateBridge and CreatePostCallback):

The function's first instructions are replaced with a jump to the bridge (safetyhook moves them
to the trampoline). The bridge saves the registers of the calling convention, records the
function's return address (by rsp at the function's entry), replaces it with the post-hook
code's address and calls the pre-hook handlers. Then it restores the registers (with the
changes the handlers made) and jumps to the trampoline, or, if a handler returned true, returns
right away. Either way the function returns to the post-hook code, which saves the registers,
calls the post-hook handlers, puts the original return address back where it was, restores the
registers and returns to it.

rax and rcx are the code's scratch registers: they are pushed first and their values saved from
the stack. Every saved register is restored before the code leaves, and the others it uses
(rdi, rsi, rdx, r8, r11 and the handlers' own) are volatile in both conventions and not
arguments at those points.
*/
namespace
{
#ifdef _WIN32
	// Microsoft x64: the first arguments in rcx, rdx and r8; the caller reserves 32 bytes of
	// shadow space for the callee.
	const Gp s_Arg0 = rcx;
	const Gp s_Arg1 = rdx;
	const Gp s_Arg2 = r8;
	const int SHADOW_SPACE = 32;
#else
	// System V: the first arguments in rdi, rsi and rdx.
	const Gp s_Arg0 = rdi;
	const Gp s_Arg1 = rsi;
	const Gp s_Arg2 = rdx;
	const int SHADOW_SPACE = 0;
#endif

	// The pushed scratch registers: rcx at [rsp], rax at [rsp + 8]. rsp at the function's entry
	// is then rsp + SCRATCH_SIZE.
	const int SCRATCH_SIZE = 16;

	// The frame of the calls the hook's code makes, below the scratch registers: the shadow space
	// and 8 bytes, so that rsp is 16-byte aligned at each call (it is 8 mod 16 at the function's
	// entry, and still after the two pushes).
	const int CALL_FRAME = SHADOW_SPACE + 8;

	Imm Address(const void* p)
	{
		return imm(reinterpret_cast<uintptr_t>(p));
	}

	template<class Function>
	Imm FunctionAddress(Function pFunction)
	{
		return imm(reinterpret_cast<uintptr_t>(pFunction));
	}

	bool IsGp64(Register_t reg)
	{
		return reg >= RAX && reg <= R15;
	}

	bool IsXmm(Register_t reg)
	{
		return (reg >= XMM0 && reg <= XMM7) || (reg >= XMM8 && reg <= XMM15);
	}

	// Register_t's 64-bit registers are in their encoding order, as AsmJit's ids.
	Gp Gp64(Register_t reg)
	{
		return gp64(static_cast<uint32_t>(reg - RAX));
	}

	Vec Xmm(Register_t reg)
	{
		return xmm(static_cast<uint32_t>(reg <= XMM7 ? reg - XMM0 : 8 + (reg - XMM8)));
	}

	// Calls <pFunction> (rsp must be 16-byte aligned, with the shadow space reserved).
	template<class Function>
	void Call(x86::Assembler& a, Function pFunction)
	{
		a.mov(rax, FunctionAddress(pFunction));
		a.call(rax);
	}

	/*
	Saves <registers> to <pRegisters>, with the scratch registers pushed (see SCRATCH_SIZE).
	Returns false for a register the x86-64 hook can't save.
	*/
	bool SaveRegisters(x86::Assembler& a, const std::vector<Register_t>& registers, CRegisters* pRegisters)
	{
		for (Register_t reg : registers)
		{
			CRegister* pRegister = pRegisters->Get(reg);
			if (!pRegister)
				continue;

			a.mov(rax, Address(pRegister->GetAddress()));
			if (reg == RAX)
			{
				a.mov(rcx, qword_ptr(rsp, 8));
				a.mov(qword_ptr(rax), rcx);
			}
			else if (reg == RCX)
			{
				a.mov(rcx, qword_ptr(rsp));
				a.mov(qword_ptr(rax), rcx);
			}
			else if (reg == RSP)
			{
				// rsp at the function's entry
				a.lea(rcx, ptr(rsp, SCRATCH_SIZE));
				a.mov(qword_ptr(rax), rcx);
			}
			else if (IsGp64(reg))
			{
				a.mov(qword_ptr(rax), Gp64(reg));
			}
			else if (IsXmm(reg))
			{
				a.movups(xmmword_ptr(rax), Xmm(reg));
			}
			else
			{
				puts("DynamicHooks: an x86-64 hook can only save the 64-bit general purpose and the XMM registers.");
				return false;
			}
		}
		return true;
	}

	/*
	Loads <registers> from <pRegisters> (rsp itself isn't), and rax and rcx from the stack if
	they aren't among them. Only mov and movups: the flags are kept.
	*/
	void RestoreRegisters(x86::Assembler& a, const std::vector<Register_t>& registers, CRegisters* pRegisters)
	{
		CRegister* pRax = nullptr;
		bool bRcx = false;
		for (Register_t reg : registers)
		{
			CRegister* pRegister = pRegisters->Get(reg);
			if (!pRegister || reg == RSP)
				continue;

			// rax last: it holds the addresses
			if (reg == RAX)
			{
				pRax = pRegister;
				continue;
			}

			a.mov(rax, Address(pRegister->GetAddress()));
			if (IsGp64(reg))
				a.mov(Gp64(reg), qword_ptr(rax));
			else
				a.movups(Xmm(reg), xmmword_ptr(rax));

			bRcx |= (reg == RCX);
		}

		if (!bRcx)
			a.mov(rcx, qword_ptr(rsp));

		if (pRax)
		{
			a.mov(rax, Address(pRax->GetAddress()));
			a.mov(rax, qword_ptr(rax));
		}
		else
		{
			a.mov(rax, qword_ptr(rsp, 8));
		}
	}
}


// ============================================================================
// >> CHook
// ============================================================================
bool CHook::Install()
{
	if (!CreateBridge())
		return false;

	// Created disabled: the bridge jumps to the trampoline through m_pTrampoline, which is only
	// known now, so the function may only jump to the bridge once it is set.
	auto patch = safetyhook::InlineHook::create(m_pFunc, m_pBridge, safetyhook::InlineHook::StartDisabled);
	if (!patch)
		return false;

	m_pTrampoline = patch->original<void*>();
	m_pJit->patch = std::move(*patch);
	if (!m_pJit->patch.enable())
	{
		m_pJit->patch.reset();
		m_pTrampoline = nullptr;
		return false;
	}

	return true;
}


// ============================================================================
// >> The hook's code
// ============================================================================
bool CHook::CreateBridge()
{
	// The post-hook code first: the bridge writes its address.
	if (!CreatePostCallback())
		return false;

	CodeHolder code;
	code.init(m_pJit->rt.environment(), m_pJit->rt.cpu_features());
	x86::Assembler a(&code);
	std::vector<Register_t> registers = m_pCallingConvention->GetRegisters();
	Label label_override = a.new_label();

	a.push(rax);
	a.push(rcx);
	if (!SaveRegisters(a, registers, m_pRegistersPre.get()))
		return false;

	a.sub(rsp, CALL_FRAME);

	// PushReturnAddress(this, return address, rsp at the function's entry)
	const int iEntry = CALL_FRAME + SCRATCH_SIZE;
	a.mov(s_Arg0, Address(this));
	a.mov(s_Arg1, qword_ptr(rsp, iEntry));
	a.lea(s_Arg2, ptr(rsp, iEntry));
	Call(a, &CHook::PushReturnAddress);

	// The function returns to the post-hook code.
	a.mov(rax, Address(m_pNewRetAddr));
	a.mov(qword_ptr(rsp, iEntry), rax);

	// The pre-hook handlers; HookHandler returns a bool, which is only al.
	a.mov(s_Arg0, Address(this));
	a.mov(s_Arg1.r32(), imm(HOOKTYPE_PRE));
	Call(a, &CHook::HookHandler);

	a.add(rsp, CALL_FRAME);
	a.test(al, al);

	// The registers with the handlers' changes (keeps the flags).
	RestoreRegisters(a, registers, m_pRegistersPre.get());
	a.lea(rsp, ptr(rsp, SCRATCH_SIZE));
	a.jnz(label_override);

	// To the trampoline. r11 is volatile and not an argument register in either convention.
	a.mov(r11, Address(&m_pTrampoline));
	a.jmp(qword_ptr(r11));

	// A pre-hook returned true: return the value it set, to the post-hook code.
	a.bind(label_override);
	a.ret();

	return m_pJit->rt.add(&m_pBridge, &code) == Error::kOk;
}

bool CHook::CreatePostCallback()
{
	CodeHolder code;
	code.init(m_pJit->rt.environment(), m_pJit->rt.cpu_features());
	x86::Assembler a(&code);
	std::vector<Register_t> registers = m_pCallingConvention->GetRegisters();

	// The function returned here, so rsp is 8 bytes above its value at the function's entry:
	// back to that (the original return address goes back into the slot below).
	a.lea(rsp, ptr(rsp, -8));
	a.push(rax);
	a.push(rcx);
	if (!SaveRegisters(a, registers, m_pRegistersPost.get()))
		return false;

	a.sub(rsp, CALL_FRAME);

	// The post-hook handlers.
	a.mov(s_Arg0, Address(this));
	a.mov(s_Arg1.r32(), imm(HOOKTYPE_POST));
	Call(a, &CHook::HookHandler);

	// PopReturnAddress(this, rsp at the function's entry), into the return address slot.
	const int iEntry = CALL_FRAME + SCRATCH_SIZE;
	a.mov(s_Arg0, Address(this));
	a.lea(s_Arg1, ptr(rsp, iEntry));
	Call(a, &CHook::PopReturnAddress);
	a.mov(qword_ptr(rsp, iEntry), rax);

	a.add(rsp, CALL_FRAME);
	RestoreRegisters(a, registers, m_pRegistersPost.get());
	a.lea(rsp, ptr(rsp, SCRATCH_SIZE));
	a.ret();

	return m_pJit->rt.add(&m_pNewRetAddr, &code) == Error::kOk;
}

#endif // DYNAMICHOOKS_X86_64
