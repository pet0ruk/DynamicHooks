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
#include "x86MsFastcall.h"


// ============================================================================
// >> x86MsFastcall
// ============================================================================
// Whether an argument of this type is passed in a register (ecx/edx) when one is left.
static bool IsRegisterArgument(DataType_t type)
{
	switch (type)
	{
		case DATA_TYPE_FLOAT:
		case DATA_TYPE_DOUBLE:
		case DATA_TYPE_LONG_LONG:
		case DATA_TYPE_ULONG_LONG:
			return false;

		default:
			return GetDataTypeSize(type) <= 4;
	}
}

// The first two arguments that fit go in ecx and edx, the others on the stack.
static std::vector<Register_t> GetArgumentRegisters(const std::vector<DataType_t>& vecArgTypes)
{
	std::vector<Register_t> registers(vecArgTypes.size(), ESP);
	int iRegisterArgs = 0;
	for (size_t i = 0; i < vecArgTypes.size() && iRegisterArgs < 2; i++)
	{
		if (IsRegisterArgument(vecArgTypes[i]))
			registers[i] = iRegisterArgs++ == 0 ? ECX : EDX;
	}
	return registers;
}

x86MsFastcall::x86MsFastcall(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment)
	// The function pops its stack arguments.
	: x86Convention(vecArgTypes, returnType, iAlignment, GetArgumentRegisters(vecArgTypes), true)
{
}
