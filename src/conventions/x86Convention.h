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

#ifndef _X86_CONVENTION_H
#define _X86_CONVENTION_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "../convention.h"


// ============================================================================
// >> x86Convention
// ============================================================================
/*
What the 32-bit x86 conventions have in common: each argument is either in a register or on
the stack (right-to-left, after the return address), integers up to 32 bits are returned in eax,
64-bit integers in eax (low half) and edx (high half), and float/double in st0.

A convention passes, for each argument, the register it is in (or ESP if it is on the stack),
and whether the function pops its stack arguments.
*/
class x86Convention: public ICallingConvention
{
public:
	std::vector<Register_t> GetRegisters() override;
	int GetPopSize() override;
	void* GetArgumentPtr(int iIndex, CRegisters* pRegisters) override;
	void ArgumentPtrChanged(int iIndex, CRegisters* pRegisters, void* pArgumentPtr) override;
	void* GetReturnPtr(CRegisters* pRegisters) override;
	void ReturnPtrChanged(CRegisters* pRegisters, void* pReturnPtr) override;

protected:
	x86Convention(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment,
		std::vector<Register_t> vecArgRegisters, bool bCalleePops);

private:
	bool ReturnsFloat() const;
	bool ReturnsInEaxEdx() const;

private:
	// Per argument: its register, or ESP if it is on the stack at m_vecOffsets[i] bytes from
	// esp (at function entry, so after the return address).
	std::vector<Register_t> m_vecArgRegisters;
	std::vector<int> m_vecOffsets;
	int m_iPopSize;

	// A 64-bit return value, put together from eax and edx.
	alignas(8) unsigned char m_ReturnBuffer[8];
};

#endif // _X86_CONVENTION_H
