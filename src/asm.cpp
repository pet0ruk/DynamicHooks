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
#include "asm.h"


// ============================================================================
// >> GetInstructionLength
// ============================================================================
static bool IsPrefix(unsigned char byte)
{
	switch (byte)
	{
		case 0xF0: case 0xF2: case 0xF3:						// lock, repne, rep
		case 0x2E: case 0x36: case 0x3E: case 0x26:				// cs, ss, ds, es
		case 0x64: case 0x65:									// fs, gs
		case 0x66: case 0x67:									// operand, address size
			return true;
	}
	return false;
}

int GetInstructionLength(const unsigned char* p)
{
	const unsigned char* start = p;
	if (*p == 0xCC)
		return 0;

	int operandSize = 4;
	bool bFPU = false;
	bool bTwoByte = false;

	// Prefixes. An x87 escape byte (D8-DF) is followed by a ModRM-like byte, decoded below as
	// the "opcode".
	while (IsPrefix(*p) || (*p & 0xF8) == 0xD8)
	{
		if (*p == 0x66)
			operandSize = 2;

		if ((*p & 0xF8) == 0xD8)
		{
			bFPU = true;
			p++;
			break;
		}
		p++;
	}

	// Two-byte opcode
	if (*p == 0x0F)
	{
		bTwoByte = true;
		p++;
	}

	unsigned char opcode = *p++;

	// ModRM
	unsigned char modRM = 0xFF;
	if (bFPU)
	{
		if ((opcode & 0xC0) != 0xC0)
			modRM = opcode;
	}
	else if (!bTwoByte)
	{
		if ((opcode & 0xC4) == 0x00 ||
			((opcode & 0xF4) == 0x60 && ((opcode & 0x0A) == 0x02 || (opcode & 0x09) == 0x09)) ||
			(opcode & 0xF0) == 0x80 ||
			((opcode & 0xF8) == 0xC0 && (opcode & 0x0E) != 0x02) ||
			(opcode & 0xFC) == 0xD0 ||
			(opcode & 0xF6) == 0xF6)
		{
			modRM = *p++;
		}
	}
	else
	{
		bool bNoModRM = ((opcode & 0xF0) == 0x00 && (opcode & 0x0F) >= 0x04 && (opcode & 0x0D) != 0x0D) ||
			(opcode & 0xF0) == 0x30 ||
			opcode == 0x77 ||
			(opcode & 0xF0) == 0x80 ||
			((opcode & 0xF0) == 0xA0 && (opcode & 0x07) <= 0x02) ||
			(opcode & 0xF8) == 0xC8;

		if (!bNoModRM)
			modRM = *p++;
	}

	// SIB
	if ((modRM & 0x07) == 0x04 && (modRM & 0xC0) != 0xC0)
		p++;

	// Displacement: dword with no base, byte, or dword
	if ((modRM & 0xC5) == 0x05)
		p += 4;
	if ((modRM & 0xC0) == 0x40)
		p += 1;
	if ((modRM & 0xC0) == 0x80)
		p += 4;

	// Immediate (x87 instructions have none)
	if (bFPU)
		return static_cast<int>(p - start);

	if (!bTwoByte)
	{
		if ((opcode & 0xC7) == 0x04 ||
			(opcode & 0xFE) == 0x6A ||					// push/imul imm8
			(opcode & 0xF0) == 0x70 ||					// jcc rel8
			opcode == 0x80 ||
			opcode == 0x83 ||
			(opcode & 0xFD) == 0xA0 ||					// mov
			opcode == 0xA8 ||							// test
			(opcode & 0xF8) == 0xB0 ||					// mov r8, imm8
			(opcode & 0xFE) == 0xC0 ||					// rcl etc.
			opcode == 0xC6 ||							// mov r/m8, imm8
			opcode == 0xCD ||							// int
			(opcode & 0xFE) == 0xD4 ||					// aad/aam
			(opcode & 0xF8) == 0xE0 ||					// loop/jcxz
			opcode == 0xEB ||							// jmp rel8
			(opcode == 0xF6 && (modRM & 0x30) == 0x00))	// test r/m8, imm8
		{
			p += 1;
		}
		else if ((opcode & 0xF7) == 0xC2)				// ret imm16, retf imm16
		{
			p += 2;
		}
		else if ((opcode & 0xFC) == 0x80 ||
			(opcode & 0xC7) == 0x05 ||
			(opcode & 0xF8) == 0xB8 ||					// mov r32, imm32
			(opcode & 0xFE) == 0xE8 ||					// call/jmp rel32
			(opcode & 0xFE) == 0x68 ||
			(opcode & 0xFC) == 0xA0 ||
			(opcode & 0xEE) == 0xA8 ||
			opcode == 0xC7 ||
			(opcode == 0xF7 && (modRM & 0x30) == 0x00))
		{
			p += operandSize;
		}
	}
	else
	{
		if (opcode == 0xBA ||							// bt etc. imm8
			opcode == 0x0F ||							// 3DNow!
			(opcode & 0xFC) == 0x70 ||					// pshufw, psllw etc. imm8
			(opcode & 0xF7) == 0xA4 ||					// shld/shrd imm8
			opcode == 0xC2 ||
			opcode == 0xC4 ||
			opcode == 0xC5 ||
			opcode == 0xC6)
		{
			p += 1;
		}
		else if ((opcode & 0xF0) == 0x80)				// jcc rel32
		{
			p += operandSize;
		}
	}

	return static_cast<int>(p - start);
}
