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

#ifndef _MANAGER_H
#define _MANAGER_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <memory>
#include <vector>

#include "hook.h"
#include "convention.h"


// ============================================================================
// >> CHookManager
// ============================================================================
class CHookManager
{
public:
	/*
	Hooks the function at <pFunc> and returns its hook. If the function is already hooked, the
	existing hook is returned (<pConvention> isn't used).

	Returns nullptr if the function can't be hooked (see CHook::Install).

	The hook keeps a reference to <pConvention> until the function is unhooked.
	*/
	CHook* HookFunction(void* pFunc, std::shared_ptr<ICallingConvention> pConvention);

	/*
	Removes the hook of the function at <pFunc> (and its handlers) and restores the function.
	*/
	void UnhookFunction(void* pFunc);

	/*
	Returns the hook of the function at <pFunc>, or nullptr if it isn't hooked.
	*/
	CHook* FindHook(void* pFunc) const;

	/*
	Removes every hook and restores all hooked functions.
	*/
	void UnhookAllFunctions();

private:
	std::vector<std::unique_ptr<CHook> > m_Hooks;
};


// ============================================================================
// >> GetHookManager
// ============================================================================
/*
Returns the hook manager.
*/
CHookManager* GetHookManager();

#endif // _MANAGER_H
