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
* x86_64MicrosoftDefault follows the design of the convention of the same name in
* SourceMod's DynamicHooks-x86_64 (extensions/dhooks/DynamicHooks), released under the
* same license:
*   Copyright (C) 2024 Benoist "Kenzzer" Andre. All rights reserved.
*   Copyright (C) 2024 AlliedModders LLC.  All rights reserved.
* This is an altered version, written for this library's ICallingConvention.
*/

#ifndef _X86_64_MICROSOFT_DEFAULT_H
#define _X86_64_MICROSOFT_DEFAULT_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "x86_64Convention.h"


// ============================================================================
// >> CLASSES
// ============================================================================
/*
Source: Microsoft's x64 calling convention (the convention of every function on 64-bit
Windows, member functions included: this is the first argument).

Registers:
	- rax = return value (integers, pointers)
	- xmm0 = return value (float, double)
	- rsp = stack pointer

Parameter passing:
	- the first four arguments by position: rcx/xmm0, rdx/xmm1, r8/xmm2, r9/xmm3 (the XMM
	  register for float and double, the general purpose one otherwise)
	- the caller reserves 32 bytes of shadow space after the return address for them; the
	  arguments after the fourth are on the stack after it, one 8-byte slot each
	- caller cleans up the stack

Arguments that are structures passed by value (not a DataType_t) aren't supported.
*/
class x86_64MicrosoftDefault: public x86_64Convention
{
public:
	x86_64MicrosoftDefault(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment = 8);
};

#endif // _X86_64_MICROSOFT_DEFAULT_H
