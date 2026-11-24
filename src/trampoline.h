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

#ifndef _TRAMPOLINE_H
#define _TRAMPOLINE_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <stddef.h>
#include <stdint.h>
#include <vector>


// ============================================================================
// >> FUNCTIONS
// ============================================================================
/*
Builds the trampoline of a hook: the whole instructions at the start of <pFunc> that cover at
least <iMinSize> bytes (the bytes the hook's jump overwrites), followed by a jump to the rest of
the function, encoded to run at address <iDest>.

Relative branches are relocated: calls, jumps and conditional jumps get 32-bit offsets to their
original targets (short jumps are widened), and a call to a GCC -fPIC thunk
(__x86.get_pc_thunk.<reg>) becomes "mov <reg>, <original return address>".

Returns false if the start of the function can't be moved safely:
- it ends (ret or an unconditional jump) or reaches int3 padding before <iMinSize> bytes;
- it contains loop/jecxz, or a relative branch with a prefix;
- a relative branch targets the bytes the hook overwrites.

The size of <code> doesn't depend on <iDest>, so a caller can build it once to learn the size,
allocate the memory and build it again for that address.

<iSourceSize> receives the number of bytes of <pFunc> the trampoline replaces.
*/
bool BuildTrampoline(unsigned char* pFunc, size_t iMinSize, uintptr_t iDest,
	std::vector<unsigned char>& code, size_t& iSourceSize);

#endif // _TRAMPOLINE_H
