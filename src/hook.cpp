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
// >> INCLUDES
// ============================================================================
#include "hook.h"
#include "utilities.h"
#include "asm.h"
#include "trampoline.h"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <asmjit/x86.h>
using namespace asmjit;
using namespace asmjit::x86;

struct CHookJit
{
	JitRuntime rt;
};

struct CHookAssembler
{
	x86::Assembler& a;
};


// ============================================================================
// >> HELPERS
// ============================================================================
static Imm Address(const void* p)
{
	return imm(reinterpret_cast<uintptr_t>(p));
}

static Mem AbsolutePtr(const void* p, uint32_t size)
{
	return ptr_abs(reinterpret_cast<uintptr_t>(p), size);
}

template<class Function>
static Imm FunctionAddress(Function pFunction)
{
	return imm(reinterpret_cast<uintptr_t>(pFunction));
}

/*
The AsmJit register of every Register_t up to GS, in the enum's order (the FPU registers are
saved with fstp/fld instead).
*/
static const Reg s_Registers[] = {
	al, cl, dl, bl, ah, ch, dh, bh,
	ax, cx, dx, bx, sp, bp, si, di,
	eax, ecx, edx, ebx, esp, ebp, esi, edi,
	mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7,
	xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7,
	cs, ss, ds, es, fs, gs,
};
static_assert(sizeof(s_Registers) / sizeof(s_Registers[0]) == GS + 1, "a Register_t is missing");

// The instruction that moves <reg> to or from memory.
static InstId GetMoveInstruction(Register_t reg)
{
	if (reg >= MM0 && reg <= MM7)
		return Inst::kIdMovq;
	if (reg >= XMM0 && reg <= XMM7)
		return Inst::kIdMovaps;

	return Inst::kIdMov;
}


// ============================================================================
// >> CHook
// ============================================================================
CHook::CHook(void* pFunc, std::shared_ptr<ICallingConvention> pConvention)
	: m_pFunc(pFunc),
	  m_pCallingConvention(std::move(pConvention)),
	  m_pJit(new CHookJit),
	  m_pBridge(nullptr),
	  m_pNewRetAddr(nullptr),
	  m_pTrampoline(nullptr),
	  m_bUsePreRegisters(false)
{
	std::vector<Register_t> registers = m_pCallingConvention->GetRegisters();
	m_pRegistersPre.reset(new CRegisters(registers));
	m_pRegistersPost.reset(new CRegisters(registers));
}

bool CHook::Install()
{
	unsigned char* pTarget = static_cast<unsigned char*>(m_pFunc);
	JitAllocator& allocator = m_pJit->rt.allocator();

	// Build the trampoline (the instructions the jump to the bridge replaces, then a jump to the
	// rest of the function) once to learn its size, then for the memory allocated for it.
	std::vector<unsigned char> code;
	size_t iSourceSize;
	if (!BuildTrampoline(pTarget, OP_JMP_SIZE, 0, code, iSourceSize))
		return false;

	JitAllocator::Span span;
	if (allocator.alloc(Out(span), code.size()) != Error::kOk)
		return false;

	m_pTrampoline = span.rx();
	if (!BuildTrampoline(pTarget, OP_JMP_SIZE, reinterpret_cast<uintptr_t>(span.rx()), code, iSourceSize)
		|| allocator.write(span, 0, code.data(), code.size()) != Error::kOk)
	{
		return false;
	}

	if (!CreateBridge())
		return false;

	// Jump to the bridge; the rest of the replaced bytes become nops.
	std::vector<unsigned char> patch(iSourceSize, OP_NOP);
	patch[0] = OP_JMP;
	WriteInt32(&patch[1], static_cast<int32_t>(
		reinterpret_cast<uintptr_t>(m_pBridge) - (reinterpret_cast<uintptr_t>(pTarget) + OP_JMP_SIZE)));

	m_OriginalBytes.assign(pTarget, pTarget + iSourceSize);
	if (!PatchCode(pTarget, patch.data(), patch.size()))
	{
		m_OriginalBytes.clear();
		return false;
	}

	return true;
}

CHook::~CHook()
{
	// Restore the function. The hook's code is freed with m_pJit.
	if (!m_OriginalBytes.empty())
		PatchCode(m_pFunc, m_OriginalBytes.data(), m_OriginalBytes.size());
}

void CHook::AddCallback(HookType_t eHookType, HookHandlerFn pCallback)
{
	if (pCallback && !IsCallbackRegistered(eHookType, pCallback))
		m_Handlers[eHookType].push_back(pCallback);
}

void CHook::RemoveCallback(HookType_t eHookType, HookHandlerFn pCallback)
{
	std::vector<HookHandlerFn>& handlers = m_Handlers[eHookType];
	handlers.erase(std::remove(handlers.begin(), handlers.end(), pCallback), handlers.end());
}

bool CHook::IsCallbackRegistered(HookType_t eHookType, HookHandlerFn pCallback) const
{
	const std::vector<HookHandlerFn>& handlers = m_Handlers[eHookType];
	return std::find(handlers.begin(), handlers.end(), pCallback) != handlers.end();
}

CRegisters* CHook::GetRegisters()
{
	return m_bUsePreRegisters ? m_pRegistersPre.get() : m_pRegistersPost.get();
}


// ============================================================================
// >> Called by the hook's code
// ============================================================================
bool __cdecl CHook::HookHandler(CHook* pHook, HookType_t eHookType)
{
	pHook->m_bUsePreRegisters = (eHookType == HOOKTYPE_PRE);
	if (pHook->m_Handlers[eHookType].empty())
		return false;

	// Iterate over a copy: a handler may add or remove handlers.
	std::vector<HookHandlerFn> handlers = pHook->m_Handlers[eHookType];
	bool bOverride = false;
	for (HookHandlerFn handler : handlers)
	{
		if (handler(eHookType, pHook))
			bOverride = true;
	}
	return bOverride;
}

void* __cdecl CHook::PopReturnAddress(CHook* pHook, void* pESP)
{
	auto it = pHook->m_RetAddr.find(pESP);
	if (it == pHook->m_RetAddr.end() || it->second.empty())
	{
		puts("DynamicHooks: unable to find the return address of a hooked call.");
		abort();
	}

	void* pRetAddr = it->second.back();
	it->second.pop_back();
	if (it->second.empty())
		pHook->m_RetAddr.erase(it);

	return pRetAddr;
}

void __cdecl CHook::PushReturnAddress(CHook* pHook, void* pRetAddr, void* pESP)
{
	pHook->m_RetAddr[pESP].push_back(pRetAddr);
}

void* CHook::FindReturnAddress(void* pESP) const
{
	auto it = m_RetAddr.find(pESP);
	if (it == m_RetAddr.end() || it->second.empty())
		return nullptr;

	return it->second.back();
}


// ============================================================================
// >> The hook's code
// ============================================================================
bool CHook::CreateBridge()
{
	CodeHolder code;
	code.init(m_pJit->rt.environment(), m_pJit->rt.cpu_features());
	x86::Assembler a(&code);
	CHookAssembler assembler{a};

	Label label_override = a.new_label();

	// Write a redirect to the post-hook code
	Write_ModifyReturnAddress(assembler);
	if (!m_pNewRetAddr)
		return false;

	// Call the pre-hook handlers; skip the function if one returned true. HookHandler returns a
	// bool, which is only al.
	Write_CallHandler(assembler, HOOKTYPE_PRE, m_pRegistersPre.get());
	a.test(al, al);

	// Restore the previously saved registers, so any changes will be applied
	// (not st0: the x87 stack is empty at function entry and must stay so). mov doesn't
	// change the flags.
	Write_RestoreRegisters(assembler, m_pRegistersPre.get(), false);

	a.jnz(label_override);

	// Jump to the trampoline
	a.jmp(Address(m_pTrampoline));

	// This code will be executed if a pre-hook returns true
	a.bind(label_override);

	// The original function is skipped, so load the return value the pre-hook set into st0
	// if the function returns a floating-point value.
	Write_LoadST0(assembler, m_pRegistersPre.get());

	// Finally, return to the caller
	// This will still call post hooks, but will skip the original function.
	a.ret(imm(m_pCallingConvention->GetPopSize()));

	return m_pJit->rt.add(&m_pBridge, &code) == Error::kOk;
}

void CHook::Write_ModifyReturnAddress(CHookAssembler& assembler)
{
	x86::Assembler& a = assembler.a;

	// Save scratch registers that are used by PushReturnAddress
	static void* pEAX = nullptr;
	static void* pECX = nullptr;
	static void* pEDX = nullptr;
	a.mov(AbsolutePtr(&pEAX, 4), eax);
	a.mov(AbsolutePtr(&pECX, 4), ecx);
	a.mov(AbsolutePtr(&pEDX, 4), edx);

	// Store the return address in eax
	a.mov(eax, dword_ptr(esp));

	// Save the original return address by using the current esp as the key.
	// This should be unique until we have returned to the original caller.
	a.push(esp);
	a.push(eax);
	a.push(Address(this));
	a.call(FunctionAddress(&CHook::PushReturnAddress));
	a.add(esp, 12);

	// Restore scratch registers
	a.mov(eax, AbsolutePtr(&pEAX, 4));
	a.mov(ecx, AbsolutePtr(&pECX, 4));
	a.mov(edx, AbsolutePtr(&pEDX, 4));

	// Override the return address. This is a redirect to our post-hook code
	if (CreatePostCallback())
		a.mov(dword_ptr(esp), Address(m_pNewRetAddr));
	else
		m_pNewRetAddr = nullptr;
}

bool CHook::CreatePostCallback()
{
	CodeHolder code;
	code.init(m_pJit->rt.environment(), m_pJit->rt.cpu_features());
	x86::Assembler a(&code);
	CHookAssembler assembler{a};

	int iPopSize = m_pCallingConvention->GetPopSize();

	// Subtract the previously added bytes (stack size + return address), so
	// that we can access the arguments again
	a.sub(esp, imm(iPopSize + 4));

	// Call the post-hook handler
	Write_CallHandler(assembler, HOOKTYPE_POST, m_pRegistersPost.get());

	// Restore the previously saved registers, so any changes will be applied
	Write_RestoreRegisters(assembler, m_pRegistersPost.get(), true);

	// Save scratch registers that are used by PopReturnAddress
	static void* pEAX = nullptr;
	static void* pECX = nullptr;
	static void* pEDX = nullptr;
	a.mov(AbsolutePtr(&pEAX, 4), eax);
	a.mov(AbsolutePtr(&pECX, 4), ecx);
	a.mov(AbsolutePtr(&pEDX, 4), edx);

	// Get the original return address
	a.push(esp);
	a.push(Address(this));
	a.call(FunctionAddress(&CHook::PopReturnAddress));
	a.add(esp, 8);

	// Save the original return address
	static void* pRetAddr = nullptr;
	a.mov(AbsolutePtr(&pRetAddr, 4), eax);

	// Restore scratch registers
	a.mov(eax, AbsolutePtr(&pEAX, 4));
	a.mov(ecx, AbsolutePtr(&pECX, 4));
	a.mov(edx, AbsolutePtr(&pEDX, 4));

	// Add the bytes again to the stack (stack size + return address), so we
	// don't corrupt the stack.
	a.add(esp, imm(iPopSize + 4));

	// Jump to the original return address
	a.jmp(AbsolutePtr(&pRetAddr, 4));

	// Generate the code
	return m_pJit->rt.add(&m_pNewRetAddr, &code) == Error::kOk;
}

void CHook::Write_CallHandler(CHookAssembler& assembler, HookType_t type, CRegisters* pRegisters)
{
	x86::Assembler& a = assembler.a;

	// Save the registers so that we can access them in our handlers
	Write_SaveRegisters(assembler, pRegisters, type == HOOKTYPE_POST);

	// Call the hook handler
	// Subtract 4 bytes to preserve 16-Byte stack alignment for Linux
	a.sub(esp, 4);
	a.push(imm(type));
	a.push(Address(this));
	a.call(FunctionAddress(&CHook::HookHandler));
	a.add(esp, 12);
}

void CHook::Write_SaveRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0)
{
	x86::Assembler& a = assembler.a;
	for (Register_t reg : m_pCallingConvention->GetRegisters())
	{
		CRegister* pRegister = pRegisters->Get(reg);
		if (!pRegister)
			continue;

		if (reg <= GS)
		{
			Mem target = AbsolutePtr(pRegister->GetAddress(), pRegister->GetSize());
			a.emit(GetMoveInstruction(reg), target, s_Registers[reg]);
		}
		else if (reg == ST0)
		{
			// The return value: pops it off the x87 stack (Write_RestoreRegisters loads it again).
			if (!bWithST0)
				continue;

			switch (GetDataTypeSize(m_pCallingConvention->m_returnType))
			{
				case SIZE_DWORD: a.fstp(AbsolutePtr(pRegister->GetAddress(), SIZE_DWORD)); break;
				case SIZE_QWORD: a.fstp(AbsolutePtr(pRegister->GetAddress(), SIZE_QWORD)); break;
				case SIZE_TWORD: a.fstp(AbsolutePtr(pRegister->GetAddress(), SIZE_TWORD)); break;
			}
		}
		else
		{
			puts("DynamicHooks: only st0 of the FPU registers can be saved.");
		}
	}
}

void CHook::Write_RestoreRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0)
{
	x86::Assembler& a = assembler.a;
	for (Register_t reg : m_pCallingConvention->GetRegisters())
	{
		CRegister* pRegister = pRegisters->Get(reg);
		if (!pRegister)
			continue;

		// cs can't be loaded with mov.
		if (reg <= GS && reg != CS)
		{
			Mem source = AbsolutePtr(pRegister->GetAddress(), pRegister->GetSize());
			a.emit(GetMoveInstruction(reg), s_Registers[reg], source);
		}
		else if (reg == ST0 && bWithST0)
		{
			Write_LoadST0(assembler, pRegisters);
		}
	}
}

void CHook::Write_LoadST0(CHookAssembler& assembler, CRegisters* pRegisters)
{
	x86::Assembler& a = assembler.a;
	DataType_t returnType = m_pCallingConvention->m_returnType;
	if (returnType != DATA_TYPE_FLOAT && returnType != DATA_TYPE_DOUBLE)
		return;

	void* pAddress = pRegisters->Get(ST0)->GetAddress();
	switch (GetDataTypeSize(returnType))
	{
		case SIZE_DWORD: a.fld(AbsolutePtr(pAddress, SIZE_DWORD)); break;
		case SIZE_QWORD: a.fld(AbsolutePtr(pAddress, SIZE_QWORD)); break;
		case SIZE_TWORD: a.fld(AbsolutePtr(pAddress, SIZE_TWORD)); break;
	}
}
