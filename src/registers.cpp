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
#include "registers.h"


// ============================================================================
// >> GetRegisterSize
// ============================================================================
int GetRegisterSize(Register_t reg)
{
	if (reg <= BH)
		return SIZE_BYTE;
	if (reg <= DI)
		return SIZE_WORD;
	if (reg <= EDI)
		return SIZE_DWORD;
	if (reg <= MM7)
		return SIZE_QWORD;
	if (reg <= XMM7)
		return SIZE_XMMWORD;
	if (reg <= GS)
		return SIZE_WORD;

	return SIZE_TWORD;
}


// ============================================================================
// >> CRegisters
// ============================================================================
CRegisters::CRegisters(const std::vector<Register_t>& registers)
{
	for (Register_t reg : registers)
	{
		if (!m_Registers[reg])
			m_Registers[reg].reset(new CRegister(GetRegisterSize(reg)));
	}
}
