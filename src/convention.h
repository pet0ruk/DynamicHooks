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

#ifndef _CONVENTION_H
#define _CONVENTION_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <vector>

#include "registers.h"


// ============================================================================
// >> DataType_t
// ============================================================================
enum DataType_t
{
	DATA_TYPE_VOID,
	DATA_TYPE_BOOL,
	DATA_TYPE_CHAR,
	DATA_TYPE_UCHAR,
	DATA_TYPE_SHORT,
	DATA_TYPE_USHORT,
	DATA_TYPE_INT,
	DATA_TYPE_UINT,
	DATA_TYPE_LONG,
	DATA_TYPE_ULONG,
	DATA_TYPE_LONG_LONG,
	DATA_TYPE_ULONG_LONG,
	DATA_TYPE_FLOAT,
	DATA_TYPE_DOUBLE,
	DATA_TYPE_POINTER,
	DATA_TYPE_STRING
};


// ============================================================================
// >> FUNCTIONS
// ============================================================================
/*
Returns <size> rounded up to a multiple of <alignment>.
*/
inline int Align(int size, int alignment)
{
	int unaligned = size % alignment;
	if (unaligned == 0)
		return size;

	return size + (alignment - unaligned);
}

/*
Returns the size of a value of the given type, rounded up to a multiple of <alignment>.
*/
inline int GetDataTypeSize(DataType_t type, int iAlignment = 4)
{
	switch (type)
	{
		case DATA_TYPE_VOID:		return 0;
		case DATA_TYPE_BOOL:		return Align(sizeof(bool), iAlignment);
		case DATA_TYPE_CHAR:		return Align(sizeof(char), iAlignment);
		case DATA_TYPE_UCHAR:		return Align(sizeof(unsigned char), iAlignment);
		case DATA_TYPE_SHORT:		return Align(sizeof(short), iAlignment);
		case DATA_TYPE_USHORT:		return Align(sizeof(unsigned short), iAlignment);
		case DATA_TYPE_INT:			return Align(sizeof(int), iAlignment);
		case DATA_TYPE_UINT:		return Align(sizeof(unsigned int), iAlignment);
		case DATA_TYPE_LONG:		return Align(sizeof(long), iAlignment);
		case DATA_TYPE_ULONG:		return Align(sizeof(unsigned long), iAlignment);
		case DATA_TYPE_LONG_LONG:	return Align(sizeof(long long), iAlignment);
		case DATA_TYPE_ULONG_LONG:	return Align(sizeof(unsigned long long), iAlignment);
		case DATA_TYPE_FLOAT:		return Align(sizeof(float), iAlignment);
		case DATA_TYPE_DOUBLE:		return Align(sizeof(double), iAlignment);
		case DATA_TYPE_POINTER:		return Align(sizeof(void *), iAlignment);
		case DATA_TYPE_STRING:		return Align(sizeof(char *), iAlignment);
	}
	return 0;
}


// ============================================================================
// >> ICallingConvention
// ============================================================================
/*
The base class of every calling convention. A convention says which registers a hook saves,
where the arguments and the return value are, and how many bytes the function pops.

Hooks hold their convention with a std::shared_ptr (see CHookManager::HookFunction).
*/
class ICallingConvention
{
public:
	/*
	@param <vecArgTypes>: The types of the function's arguments.
	@param <returnType>: The function's return type.
	@param <iAlignment>: The stack alignment of the arguments.
	*/
	ICallingConvention(std::vector<DataType_t> vecArgTypes, DataType_t returnType, int iAlignment = 4)
		: m_vecArgTypes(std::move(vecArgTypes)), m_returnType(returnType), m_iAlignment(iAlignment)
	{
	}

	virtual ~ICallingConvention() = default;

	ICallingConvention(const ICallingConvention&) = delete;
	ICallingConvention& operator=(const ICallingConvention&) = delete;

	/*
	The registers a hook saves before calling its handlers (and restores afterwards).
	*/
	virtual std::vector<Register_t> GetRegisters() = 0;

	/*
	The number of bytes the function removes from the stack when it returns.
	*/
	virtual int GetPopSize() = 0;

	/*
	Returns a pointer to the argument at <iIndex>, given the saved registers.
	*/
	virtual void* GetArgumentPtr(int iIndex, CRegisters* pRegisters) = 0;

	/*
	Called after the argument at <iIndex>, at <pArgumentPtr>, was changed.
	*/
	virtual void ArgumentPtrChanged(int iIndex, CRegisters* pRegisters, void* pArgumentPtr) = 0;

	/*
	Returns a pointer to the return value, given the saved registers.
	*/
	virtual void* GetReturnPtr(CRegisters* pRegisters) = 0;

	/*
	Called after the return value, at <pReturnPtr>, was changed.
	*/
	virtual void ReturnPtrChanged(CRegisters* pRegisters, void* pReturnPtr) = 0;

public:
	std::vector<DataType_t> m_vecArgTypes;
	DataType_t m_returnType;
	int m_iAlignment;
};

#endif // _CONVENTION_H
