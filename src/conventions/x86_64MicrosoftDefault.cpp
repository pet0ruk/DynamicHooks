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

// ============================================================================
// >> INCLUDES
// ============================================================================
#include "x86_64MicrosoftDefault.h"


// ============================================================================
// >> HELPERS
// ============================================================================
namespace
{
	const Register_t s_IntegerRegisters[] = { RCX, RDX, R8, R9 };
	const Register_t s_VectorRegisters[] = { XMM0, XMM1, XMM2, XMM3 };

	std::vector<Register_t> GetArgRegisters(const std::vector<DataType_t>& vecArgTypes)
	{
		std::vector<Register_t> registers;
		for (size_t i = 0; i < vecArgTypes.size(); i++)
		{
			if (i >= 4)
				registers.push_back(RSP);
			else if (x86_64Convention::IsFloatingPoint(vecArgTypes[i]))
				registers.push_back(s_VectorRegisters[i]);
			else
				registers.push_back(s_IntegerRegisters[i]);
		}
		return registers;
	}

	// Argument i's stack slot (the shadow space has one for each of the first four) is
	// 8 * (i + 1) bytes from rsp at the function's entry, after the return address.
	std::vector<int> GetOffsets(const std::vector<DataType_t>& vecArgTypes)
	{
		std::vector<int> offsets;
		for (size_t i = 0; i < vecArgTypes.size(); i++)
			offsets.push_back(i >= 4 ? static_cast<int>(8 * (i + 1)) : 0);
		return offsets;
	}
}


// ============================================================================
// >> x86_64MicrosoftDefault
// ============================================================================
x86_64MicrosoftDefault::x86_64MicrosoftDefault(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment)
	// (vecArgTypes isn't moved: the other arguments are computed from it)
	: x86_64Convention(vecArgTypes, returnType, iAlignment, GetArgRegisters(vecArgTypes), GetOffsets(vecArgTypes))
{
}
