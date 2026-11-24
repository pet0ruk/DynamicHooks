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
#include "x86_64SystemVDefault.h"


// ============================================================================
// >> HELPERS
// ============================================================================
namespace
{
	const Register_t s_IntegerRegisters[] = { RDI, RSI, RDX, RCX, R8, R9 };
	const Register_t s_VectorRegisters[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7 };

	// The register of every argument (RSP: on the stack), and the offset of the stack ones.
	struct Layout
	{
		std::vector<Register_t> registers;
		std::vector<int> offsets;
	};

	Layout GetLayout(const std::vector<DataType_t>& vecArgTypes)
	{
		Layout layout;
		size_t iInteger = 0, iVector = 0;
		int iOffset = 8;	// after the return address
		for (DataType_t type : vecArgTypes)
		{
			Register_t reg = RSP;
			if (x86_64Convention::IsFloatingPoint(type))
			{
				if (iVector < sizeof(s_VectorRegisters) / sizeof(s_VectorRegisters[0]))
					reg = s_VectorRegisters[iVector++];
			}
			else if (iInteger < sizeof(s_IntegerRegisters) / sizeof(s_IntegerRegisters[0]))
			{
				reg = s_IntegerRegisters[iInteger++];
			}

			layout.registers.push_back(reg);
			layout.offsets.push_back(reg == RSP ? iOffset : 0);
			if (reg == RSP)
				iOffset += 8;
		}
		return layout;
	}
}


// ============================================================================
// >> x86_64SystemVDefault
// ============================================================================
x86_64SystemVDefault::x86_64SystemVDefault(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment)
	// (vecArgTypes isn't moved: the other arguments are computed from it)
	: x86_64Convention(vecArgTypes, returnType, iAlignment,
		GetLayout(vecArgTypes).registers, GetLayout(vecArgTypes).offsets)
{
}
