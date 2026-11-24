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

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "x86_64Convention.h"

#include <algorithm>


// ============================================================================
// >> x86_64Convention
// ============================================================================
x86_64Convention::x86_64Convention(std::vector<DataType_t> vecArgTypes, DataType_t returnType,
	int iAlignment, std::vector<Register_t> vecArgRegisters, std::vector<int> vecOffsets)
	: ICallingConvention(std::move(vecArgTypes), returnType, iAlignment),
	  m_vecArgRegisters(std::move(vecArgRegisters)),
	  m_vecOffsets(std::move(vecOffsets))
{
}

std::vector<Register_t> x86_64Convention::GetRegisters()
{
	std::vector<Register_t> registers;
	auto add = [&registers](Register_t reg) {
		if (std::find(registers.begin(), registers.end(), reg) == registers.end())
			registers.push_back(reg);
	};

	add(RSP);
	add(RAX);
	for (Register_t reg : m_vecArgRegisters)
		add(reg);

	if (IsFloatingPoint(m_returnType))
		add(XMM0);

	return registers;
}

int x86_64Convention::GetPopSize()
{
	return 0;
}

void* x86_64Convention::GetArgumentPtr(int iIndex, CRegisters* pRegisters)
{
	Register_t reg = m_vecArgRegisters[iIndex];
	if (reg != RSP)
		return pRegisters->Get(reg)->GetAddress();

	return reinterpret_cast<void*>(pRegisters->Get(RSP)->GetValue<uintptr_t>() + m_vecOffsets[iIndex]);
}

void x86_64Convention::ArgumentPtrChanged(int /* iIndex */, CRegisters* /* pRegisters */, void* /* pArgumentPtr */)
{
	// GetArgumentPtr points at the saved register or the stack slot itself.
}

void* x86_64Convention::GetReturnPtr(CRegisters* pRegisters)
{
	return pRegisters->Get(IsFloatingPoint(m_returnType) ? XMM0 : RAX)->GetAddress();
}

void x86_64Convention::ReturnPtrChanged(CRegisters* /* pRegisters */, void* /* pReturnPtr */)
{
	// GetReturnPtr points at the saved register itself.
}
