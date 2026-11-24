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

#ifndef _HOOK_JIT_H
#define _HOOK_JIT_H

// ============================================================================
// >> INCLUDES
// ============================================================================
// Internal to DynamicHooks (only its .cpp files include this): AsmJit and, on x86-64,
// safetyhook aren't part of its interface.
#include "registers.h"

#include <asmjit/x86.h>

#ifdef DYNAMICHOOKS_X86_64
	#include <safetyhook.hpp>
#endif


// ============================================================================
// >> CHookJit
// ============================================================================
/*
What a hook allocates: the code AsmJit generates for it and, on x86-64, safetyhook's patch of
the function (its jump to the hook and the trampoline).
*/
struct CHookJit
{
	asmjit::JitRuntime rt;

#ifdef DYNAMICHOOKS_X86_64
	// Declared after rt, so it is destroyed first: the function is restored before the code
	// it jumps to is freed.
	safetyhook::InlineHook patch;
#endif
};

struct CHookAssembler
{
	asmjit::x86::Assembler& a;
};

#endif // _HOOK_JIT_H
