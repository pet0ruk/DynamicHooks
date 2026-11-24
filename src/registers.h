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

#ifndef _REGISTERS_H
#define _REGISTERS_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <array>
#include <memory>
#include <string.h>
#include <vector>


// ============================================================================
// >> RegisterSize
// ============================================================================
enum RegisterSize
{
	SIZE_INVALID = 0,
	SIZE_BYTE = 1,
	SIZE_WORD = 2,
	SIZE_DWORD = 4,
	SIZE_QWORD = 8,
	SIZE_TWORD = 10,
	SIZE_XMMWORD = 16,
	SIZE_YMMWORD = 32,
	SIZE_ZMMWORD = 64
};


// ============================================================================
// >> Register_t
// ============================================================================
enum Register_t
{
	// 8-bit general purpose registers
	AL, CL, DL, BL, AH, CH, DH, BH,

	// 16-bit general purpose registers
	AX, CX, DX, BX, SP, BP, SI, DI,

	// 32-bit general purpose registers
	EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,

	// 64-bit MMX registers
	MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,

	// 128-bit XMM registers
	XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,

	// 16-bit segment registers
	CS, SS, DS, ES, FS, GS,

	// 80-bit FPU registers (only st0 can be saved)
	ST0, ST1, ST2, ST3, ST4, ST5, ST6, ST7,
};

// The number of Register_t values.
const int REGISTER_COUNT = ST7 + 1;

// The size of a register in bytes.
int GetRegisterSize(Register_t reg);


// ============================================================================
// >> CRegister
// ============================================================================
/*
The saved value of a register. The storage is 16-byte aligned, as movaps (which saves the XMM
registers) requires.
*/
class CRegister
{
public:
	explicit CRegister(int iSize) : m_iSize(iSize)
	{
		memset(m_Value, 0, sizeof(m_Value));
	}

	CRegister(const CRegister&) = delete;
	CRegister& operator=(const CRegister&) = delete;

	int GetSize() const
	{
		return m_iSize;
	}

	void* GetAddress()
	{
		return m_Value;
	}

	template<class T>
	T GetValue() const
	{
		T value;
		memcpy(&value, m_Value, sizeof(T));
		return value;
	}

	template<class T>
	void SetValue(T value)
	{
		memcpy(m_Value, &value, sizeof(T));
	}

	// The value at <iOffset> bytes from the address the register holds.
	template<class T>
	T GetPointerValue(int iOffset = 0) const
	{
		return *reinterpret_cast<T*>(GetValue<unsigned long>() + iOffset);
	}

	template<class T>
	void SetPointerValue(T value, int iOffset = 0)
	{
		*reinterpret_cast<T*>(GetValue<unsigned long>() + iOffset) = value;
	}

private:
	int m_iSize;
	alignas(16) unsigned char m_Value[SIZE_XMMWORD];
};


// ============================================================================
// >> CRegisters
// ============================================================================
/*
The registers a calling convention saves (see ICallingConvention::GetRegisters).
*/
class CRegisters
{
public:
	explicit CRegisters(const std::vector<Register_t>& registers);

	CRegisters(const CRegisters&) = delete;
	CRegisters& operator=(const CRegisters&) = delete;

	// The saved register, or nullptr if this register isn't saved.
	CRegister* Get(Register_t reg) const
	{
		return m_Registers[reg].get();
	}

private:
	std::array<std::unique_ptr<CRegister>, REGISTER_COUNT> m_Registers;
};

#endif // _REGISTERS_H
