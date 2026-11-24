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


#ifndef _UTILITIES_H
#define _UTILITIES_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <stddef.h>
#include <stdint.h>
#include <string.h>


// ============================================================================
// >> FUNCTIONS
// ============================================================================
/*
Copies <size> bytes from <pSrc> to <pDest> in code (or other read-only) memory: makes every page
they are on writable, writes them and restores each page's previous protection.

Returns false (writing nothing) if a page isn't mapped or can't be made writable.
*/
bool PatchCode(void* pDest, const void* pSrc, size_t size);

/*
Read and write a 32-bit value at any (unaligned) address, e.g. the offset of a jump in code.
*/
inline int32_t ReadInt32(const void* p)
{
	int32_t value;
	memcpy(&value, p, sizeof(value));
	return value;
}

inline void WriteInt32(void* p, int32_t value)
{
	memcpy(p, &value, sizeof(value));
}

#endif // _UTILITIES_H
