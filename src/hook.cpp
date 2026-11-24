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
#include "hook_jit.h"
#include "utilities.h"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// What the 32-bit and the x86-64 hook have in common; the patching and the code the function
// jumps to are in hook_x86.cpp and hook_x64.cpp.


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

CHook::~CHook()
{
#ifndef DYNAMICHOOKS_X86_64
	// Restore the function. The hook's code is freed with m_pJit.
	if (!m_OriginalBytes.empty())
		PatchCode(m_pFunc, m_OriginalBytes.data(), m_OriginalBytes.size());
#endif
	// x86-64: m_pJit's patch restores the function, then its code is freed.
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
