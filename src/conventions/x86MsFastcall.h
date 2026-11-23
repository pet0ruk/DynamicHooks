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

#ifndef _X86_MS_FASTCALL_H
#define _X86_MS_FASTCALL_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "../convention.h"


// ============================================================================
// >> CLASSES
// ============================================================================
/*
Source: DynCall manual and Windows docs

Registers:
	- eax = return value
	- ecx = first register argument
	- edx = second register argument, return value
	- esp = stack pointer
	- st0 = floating point return value

Parameter passing:
	- the first two arguments, from left to right, that are 32 bits or smaller and not
	  floating-point (bool, char, short, int, long, pointer, string) are passed in ecx and edx
	- all other arguments (float, double, long long, and any after the first two register
	  arguments) are pushed onto the stack, right-to-left
	- callee cleans up the stack
	- alignment: 4 bytes

	For example, f(float a, int b, double c, int d, int e): b in ecx, d in edx; a, c and e on
	the stack.

Return values:
	- return values of pointer or intergral type (<= 32 bits) are returned via the eax register
	- integers > 32 bits are returned via the eax and edx registers
	- floating pointer types are returned via the st0 register
*/
class x86MsFastcall: public ICallingConvention
{	
public:
	x86MsFastcall(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment=4);
	~x86MsFastcall();

	virtual std::list<Register_t> GetRegisters();
	virtual int GetPopSize();
	
	virtual void* GetArgumentPtr(int iIndex, CRegisters* pRegisters);
	virtual void ArgumentPtrChanged(int iIndex, CRegisters* pRegisters, void* pArgumentPtr);

	virtual void* GetReturnPtr(CRegisters* pRegisters);
	virtual void ReturnPtrChanged(CRegisters* pRegisters, void* pReturnPtr);

private:
	void* m_pReturnBuffer;

	// Per argument: its register (ECX or EDX), or ESP if it is on the stack, at m_pOffsets[i]
	// bytes from esp (at function entry, so after the return address).
	Register_t* m_pRegisters;
	int* m_pOffsets;
	int m_iRegisterArgs;
};

#endif // _X86_MS_FASTCALL_H