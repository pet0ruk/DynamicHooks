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
#include "trampoline.h"
#include "asm.h"
#include "utilities.h"



// ============================================================================
// >> HELPERS
// ============================================================================
#define REG_ESP 4

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

static void EmitInt32(std::vector<unsigned char>& code, uint32_t value)
{
	unsigned char bytes[4];
	WriteInt32(bytes, static_cast<int32_t>(value));
	code.insert(code.end(), bytes, bytes + 4);
}

// Appends the 32-bit offset from the end of this field (the end of the instruction) to <target>.
static void EmitRel32(std::vector<unsigned char>& code, uintptr_t iDest, const unsigned char* target)
{
	uintptr_t next = iDest + code.size() + 4;
	EmitInt32(code, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target) - next));
}

/*
Returns the register a GCC -fPIC thunk at <p> loads, or -1 if <p> isn't one. A thunk is
"mov <reg>, [esp]; ret": 8B /r with ModRM 00 reg 100, then SIB 24 (no index, base esp), then C3.
*/
static int GetPcThunkRegister(const unsigned char* p)
{
#ifdef _WIN32
	return -1;
#else
	unsigned char modrm = p[1];
	int reg = (modrm >> 3) & 7;
	if (p[0] == 0x8B && (modrm & 0xC7) == 0x04 && p[2] == 0x24 && p[3] == 0xC3 && reg != REG_ESP)
		return reg;

	return -1;
#endif
}


// ============================================================================
// >> BuildTrampoline
// ============================================================================
bool BuildTrampoline(unsigned char* pFunc, size_t iMinSize, uintptr_t iDest,
	std::vector<unsigned char>& code, size_t& iSourceSize)
{
	code.clear();

	// Original targets of the relocated branches.
	std::vector<const unsigned char*> targets;

	size_t i = 0;
	bool bEnded = false;
	while (i < iMinSize)
	{
		// The function returns or jumps away before the bytes the hook needs.
		if (bEnded)
			return false;

		unsigned char* p = pFunc + i;
		size_t iPrefixes = 0;
		while (iPrefixes < 14 && IsPrefix(p[iPrefixes]))
			iPrefixes++;

		const unsigned char* q = p + iPrefixes;
		unsigned char op = q[0];

		bool bRelative = op == 0xE8 || op == 0xE9 || op == 0xEB || (op & 0xF0) == 0x70
			|| (op >= 0xE0 && op <= 0xE3) || (op == 0x0F && (q[1] & 0xF0) == 0x80);

		// int3 padding: the function is shorter than the hook's jump.
		if (op == 0xCC)
			return false;

		// A prefixed relative branch (e.g. a 16-bit offset) isn't relocated.
		if (bRelative && iPrefixes)
			return false;

		size_t iLength;
		const unsigned char* target = nullptr;
		if (op == 0xE8 || op == 0xE9)
		{
			// call/jmp rel32
			iLength = 5;
			target = q + 5 + ReadInt32(q + 1);
			int reg = op == 0xE8 ? GetPcThunkRegister(target) : -1;
			if (reg != -1)
			{
				// mov <reg>, <the address after the original call>
				code.push_back(static_cast<unsigned char>(0xB8 + reg));
				EmitInt32(code, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(q + 5)));
				target = nullptr;
			}
			else
			{
				code.push_back(op);
				EmitRel32(code, iDest, target);
			}

			if (op == 0xE9)
				bEnded = true;
		}
		else if (op == 0xEB)
		{
			// jmp rel8 -> jmp rel32
			iLength = 2;
			target = q + 2 + static_cast<int8_t>(q[1]);
			code.push_back(0xE9);
			EmitRel32(code, iDest, target);
			bEnded = true;
		}
		else if ((op & 0xF0) == 0x70)
		{
			// jcc rel8 -> jcc rel32
			iLength = 2;
			target = q + 2 + static_cast<int8_t>(q[1]);
			code.push_back(0x0F);
			code.push_back(static_cast<unsigned char>(0x80 | (op & 0x0F)));
			EmitRel32(code, iDest, target);
		}
		else if (op == 0x0F && (q[1] & 0xF0) == 0x80)
		{
			// jcc rel32
			iLength = 6;
			target = q + 6 + ReadInt32(q + 2);
			code.push_back(q[0]);
			code.push_back(q[1]);
			EmitRel32(code, iDest, target);
		}
		else if (op >= 0xE0 && op <= 0xE3)
		{
			// loop/loope/loopne/jecxz have no 32-bit form.
			return false;
		}
		else
		{
			iLength = GetInstructionLength(p);
			if (iLength == 0)
				return false;

			code.insert(code.end(), p, p + iLength);

			// ret, ret imm16, retf, retf imm16, jmp r/m32, jmp m16:32
			int reg = (q[1] >> 3) & 7;
			if (op == 0xC3 || op == 0xC2 || op == 0xCB || op == 0xCA
				|| (op == 0xFF && (reg == 4 || reg == 5)))
			{
				bEnded = true;
			}
		}

		if (target)
			targets.push_back(target);

		i += iLength;
	}

	// A branch into the replaced bytes would land in the hook's jump.
	for (size_t j = 0; j < targets.size(); j++)
	{
		if (targets[j] >= pFunc && targets[j] < pFunc + i)
			return false;
	}

	// Continue with the rest of the function, unless the copied code left it.
	if (!bEnded)
	{
		code.push_back(OP_JMP);
		EmitRel32(code, iDest, pFunc + i);
	}

	iSourceSize = i;
	return true;
}
