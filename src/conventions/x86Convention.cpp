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
#include "x86Convention.h"

#include <algorithm>
#include <string.h>


// ============================================================================
// >> x86Convention
// ============================================================================
x86Convention::x86Convention(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment,
	std::vector<Register_t> vecArgRegisters, bool bCalleePops)
	: ICallingConvention(std::move(vecArgTypes), returnType, iAlignment),
	  m_vecArgRegisters(std::move(vecArgRegisters)),
	  m_vecOffsets(m_vecArgTypes.size(), 0),
	  m_iPopSize(0)
{
	memset(m_ReturnBuffer, 0, sizeof(m_ReturnBuffer));

	// The stack arguments start after the return address.
	int iOffset = 4;
	for (size_t i = 0; i < m_vecArgTypes.size(); i++)
	{
		if (m_vecArgRegisters[i] != ESP)
			continue;

		m_vecOffsets[i] = iOffset;
		iOffset += GetDataTypeSize(m_vecArgTypes[i], m_iAlignment);
	}

	if (bCalleePops)
		m_iPopSize = iOffset - 4;
}

bool x86Convention::ReturnsFloat() const
{
	return m_returnType == DATA_TYPE_FLOAT || m_returnType == DATA_TYPE_DOUBLE;
}

bool x86Convention::ReturnsInEaxEdx() const
{
	return !ReturnsFloat() && GetDataTypeSize(m_returnType) > 4;
}

std::vector<Register_t> x86Convention::GetRegisters()
{
	std::vector<Register_t> registers;
	registers.push_back(ESP);

	auto add = [&registers](Register_t reg) {
		if (std::find(registers.begin(), registers.end(), reg) == registers.end())
			registers.push_back(reg);
	};

	for (Register_t reg : m_vecArgRegisters)
		add(reg);

	if (ReturnsFloat())
	{
		add(ST0);
	}
	else
	{
		add(EAX);
		if (ReturnsInEaxEdx())
			add(EDX);
	}
	return registers;
}

int x86Convention::GetPopSize()
{
	return m_iPopSize;
}

void* x86Convention::GetArgumentPtr(int iIndex, CRegisters* pRegisters)
{
	Register_t reg = m_vecArgRegisters[iIndex];
	if (reg != ESP)
		return pRegisters->Get(reg)->GetAddress();

	return reinterpret_cast<void*>(pRegisters->Get(ESP)->GetValue<unsigned long>() + m_vecOffsets[iIndex]);
}

void x86Convention::ArgumentPtrChanged(int /* iIndex */, CRegisters* /* pRegisters */, void* /* pArgumentPtr */)
{
	// GetArgumentPtr points at the saved register or the stack slot itself.
}

void* x86Convention::GetReturnPtr(CRegisters* pRegisters)
{
	if (ReturnsFloat())
		return pRegisters->Get(ST0)->GetAddress();

	if (ReturnsInEaxEdx())
	{
		memcpy(m_ReturnBuffer, pRegisters->Get(EAX)->GetAddress(), 4);
		memcpy(m_ReturnBuffer + 4, pRegisters->Get(EDX)->GetAddress(), 4);
		return m_ReturnBuffer;
	}

	return pRegisters->Get(EAX)->GetAddress();
}

void x86Convention::ReturnPtrChanged(CRegisters* pRegisters, void* /* pReturnPtr */)
{
	if (!ReturnsInEaxEdx())
		return;

	memcpy(pRegisters->Get(EAX)->GetAddress(), m_ReturnBuffer, 4);
	memcpy(pRegisters->Get(EDX)->GetAddress(), m_ReturnBuffer + 4, 4);
}
