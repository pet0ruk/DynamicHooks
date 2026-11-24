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

#ifndef _X86_64_SYSTEMV_DEFAULT_H
#define _X86_64_SYSTEMV_DEFAULT_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "x86_64Convention.h"


// ============================================================================
// >> CLASSES
// ============================================================================
/*
Source: System V Application Binary Interface, AMD64 Architecture Processor Supplement
(the convention of every function on 64-bit Linux, member functions included: this is the
first argument).

Registers:
	- rax = return value (integers, pointers)
	- xmm0 = return value (float, double)
	- rsp = stack pointer

Parameter passing:
	- integer and pointer arguments in rdi, rsi, rdx, rcx, r8, r9 (in this order), float and
	  double ones in xmm0 to xmm7, each kind counted separately; the rest on the stack, one
	  8-byte slot each, left to right after the return address
	- caller cleans up the stack

Arguments that are structures passed by value (not a DataType_t) aren't supported.
*/
class x86_64SystemVDefault: public x86_64Convention
{
public:
	x86_64SystemVDefault(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment = 8);
};

#endif // _X86_64_SYSTEMV_DEFAULT_H
