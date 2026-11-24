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
#include "utilities.h"

#include <string.h>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
#endif

#ifdef __linux__
	#include <stdio.h>
	#include <sys/mman.h>
	#include <unistd.h>
#endif


// ============================================================================
// >> Page protection
// ============================================================================
#ifdef __linux__
struct CMapping
{
	uintptr_t start;
	uintptr_t end;
	int prot;
};

// Reads the mappings of this process and their protection from /proc/self/maps.
static bool ReadMappings(std::vector<CMapping>& mappings)
{
	FILE* file = fopen("/proc/self/maps", "r");
	if (!file)
		return false;

	char line[512];
	while (fgets(line, sizeof(line), file))
	{
		unsigned long start, end;
		char perms[5];
		if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3)
			continue;

		CMapping mapping;
		mapping.start = start;
		mapping.end = end;
		mapping.prot = (perms[0] == 'r' ? PROT_READ : 0) | (perms[1] == 'w' ? PROT_WRITE : 0)
			| (perms[2] == 'x' ? PROT_EXEC : 0);
		mappings.push_back(mapping);
	}

	fclose(file);
	return true;
}

static bool GetPageProtection(const std::vector<CMapping>& mappings, uintptr_t page, int& prot)
{
	for (size_t i = 0; i < mappings.size(); i++)
	{
		if (page >= mappings[i].start && page < mappings[i].end)
		{
			prot = mappings[i].prot;
			return true;
		}
	}
	return false;
}
#endif


// ============================================================================
// >> PatchCode
// ============================================================================
bool PatchCode(void* pDest, const void* pSrc, size_t size)
{
	if (!size)
		return true;

#if defined __linux__
	uintptr_t iPageSize = (uintptr_t) sysconf(_SC_PAGESIZE);
	uintptr_t iFirst = (uintptr_t) pDest & ~(iPageSize - 1);
	uintptr_t iEnd = ((uintptr_t) pDest + size + iPageSize - 1) & ~(iPageSize - 1);

	std::vector<CMapping> mappings;
	if (!ReadMappings(mappings))
		return false;

	// The protection of every page the bytes are on (they may span more than one).
	std::vector<int> prots;
	for (uintptr_t page = iFirst; page < iEnd; page += iPageSize)
	{
		int prot;
		if (!GetPageProtection(mappings, page, prot))
			return false;

		prots.push_back(prot);
	}

	// Make them writable (keeping them executable: other threads may be running code there).
	size_t iUnprotected = 0;
	for (; iUnprotected < prots.size(); iUnprotected++)
	{
		void* page = (void *) (iFirst + iUnprotected * iPageSize);
		if (mprotect(page, iPageSize, prots[iUnprotected] | PROT_READ | PROT_WRITE) != 0)
			break;
	}

	bool bResult = iUnprotected == prots.size();
	if (bResult)
		memcpy(pDest, pSrc, size);

	// Restore the previous protection.
	for (size_t i = 0; i < iUnprotected; i++)
		mprotect((void *) (iFirst + i * iPageSize), iPageSize, prots[i]);

	return bResult;

#elif defined _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	uintptr_t iPageSize = info.dwPageSize;
	uintptr_t iFirst = (uintptr_t) pDest & ~(iPageSize - 1);
	uintptr_t iEnd = ((uintptr_t) pDest + size + iPageSize - 1) & ~(iPageSize - 1);

	// VirtualProtect returns the previous protection of the first page only, so do it per page.
	std::vector<DWORD> prots;
	bool bResult = true;
	for (uintptr_t page = iFirst; page < iEnd; page += iPageSize)
	{
		DWORD old;
		if (!VirtualProtect((void *) page, 1, PAGE_EXECUTE_READWRITE, &old))
		{
			bResult = false;
			break;
		}
		prots.push_back(old);
	}

	if (bResult)
	{
		memcpy(pDest, pSrc, size);
		FlushInstructionCache(GetCurrentProcess(), pDest, size);
	}

	for (size_t i = 0; i < prots.size(); i++)
	{
		DWORD old;
		VirtualProtect((void *) (iFirst + i * iPageSize), 1, prots[i], &old);
	}

	return bResult;
#endif
}
