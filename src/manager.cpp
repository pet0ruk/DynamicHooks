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
#include "manager.h"

#include <algorithm>


// ============================================================================
// >> CHookManager
// ============================================================================
CHook* CHookManager::HookFunction(void* pFunc, std::shared_ptr<ICallingConvention> pConvention)
{
	if (!pFunc || !pConvention)
		return nullptr;

	if (CHook* pHook = FindHook(pFunc))
		return pHook;

	std::unique_ptr<CHook> pHook(new CHook(pFunc, std::move(pConvention)));
	if (!pHook->Install())
		return nullptr;

	m_Hooks.push_back(std::move(pHook));
	return m_Hooks.back().get();
}

void CHookManager::UnhookFunction(void* pFunc)
{
	auto it = std::find_if(m_Hooks.begin(), m_Hooks.end(),
		[pFunc](const std::unique_ptr<CHook>& pHook) { return pHook->GetFunction() == pFunc; });

	if (it == m_Hooks.end())
		return;

	// Take the hook out of the list first, so FindHook doesn't return it while it is destroyed.
	std::unique_ptr<CHook> pHook = std::move(*it);
	m_Hooks.erase(it);
}

CHook* CHookManager::FindHook(void* pFunc) const
{
	if (!pFunc)
		return nullptr;

	for (const std::unique_ptr<CHook>& pHook : m_Hooks)
	{
		if (pHook->GetFunction() == pFunc)
			return pHook.get();
	}
	return nullptr;
}

void CHookManager::UnhookAllFunctions()
{
	std::vector<std::unique_ptr<CHook> > hooks;
	hooks.swap(m_Hooks);
}


// ============================================================================
// >> GetHookManager
// ============================================================================
CHookManager* GetHookManager()
{
	// Never destroyed: hooks may still be in use while the process exits.
	static CHookManager* s_pManager = new CHookManager;
	return s_pManager;
}
