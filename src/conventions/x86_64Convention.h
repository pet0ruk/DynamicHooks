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

#ifndef _X86_64_CONVENTION_H
#define _X86_64_CONVENTION_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "../convention.h"


// ============================================================================
// >> x86_64Convention
// ============================================================================
/*
What the x86-64 conventions (System V on Linux, Microsoft's on Windows) have in common, for
arguments and return values of the types in DataType_t (integers, pointers, float, double):
- each argument is in one register (a general purpose one, or an XMM register for float and
  double) or in one 8-byte stack slot after the return address;
- integers and pointers are returned in rax, float and double in xmm0;
- the caller removes the stack arguments.
A convention passes, for each argument, its register (or RSP if it is on the stack), and the
offset of each stack argument from rsp at the function's entry.

The registers saved always include rax: its low byte is the number of vector registers a
variadic System V function is passed.
*/
class x86_64Convention: public ICallingConvention
{
public:
	std::vector<Register_t> GetRegisters() override;
	int GetPopSize() override;
	void* GetArgumentPtr(int iIndex, CRegisters* pRegisters) override;
	void ArgumentPtrChanged(int iIndex, CRegisters* pRegisters, void* pArgumentPtr) override;
	void* GetReturnPtr(CRegisters* pRegisters) override;
	void ReturnPtrChanged(CRegisters* pRegisters, void* pReturnPtr) override;

	// Whether a value of <type> is passed and returned in an XMM register.
	static bool IsFloatingPoint(DataType_t type)
	{
		return type == DATA_TYPE_FLOAT || type == DATA_TYPE_DOUBLE;
	}

protected:
	x86_64Convention(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment,
		std::vector<Register_t> vecArgRegisters, std::vector<int> vecOffsets);

private:
	// Per argument: its register, or RSP if it is on the stack at m_vecOffsets[i] bytes from
	// rsp at the function's entry.
	std::vector<Register_t> m_vecArgRegisters;
	std::vector<int> m_vecOffsets;
};

#endif // _X86_64_CONVENTION_H
