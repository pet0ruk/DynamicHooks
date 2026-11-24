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

#ifndef _HOOK_H
#define _HOOK_H

// ============================================================================
// >> INCLUDES
// ============================================================================
#include <list>
#include <map>
#include <vector>

#include "registers.h"
#include "convention.h"


// ============================================================================
// >> HookType_t
// ============================================================================
enum HookType_t
{
	// Callback will be executed before the original function.
	HOOKTYPE_PRE,

	// Callback will be executed after the original function.
	HOOKTYPE_POST
};


// ============================================================================
// >> TYPEDEFS
// ============================================================================
class CHook;
typedef bool (*HookHandlerFn)(HookType_t, CHook*);

// Defined in hook.cpp, so that this header (and code that includes it) doesn't depend on AsmJit:
// the runtime the generated code lives in, and the assembler that writes it.
struct CHookJit;
struct CHookAssembler;

#ifdef __linux__
#define __cdecl
#endif


// ============================================================================
// >> CLASSES
// ============================================================================
class CHook
{
private:
	friend class CHookManager;

	/*
	Creates a new function hook.

	@param <pFunc>:
	The address of the function to hook

	@param <pConvention>:
	The calling convention of <pFunc>.
	*/
	CHook(void* pFunc, ICallingConvention* pConvention);
	~CHook();

	/*
	Patches the function to jump to the hook. Returns false, leaving the function unchanged, if
	its start can't be moved to a trampoline (see BuildTrampoline in trampoline.h) or memory for
	the hook can't be allocated or written.
	*/
	bool Install();

public:
	/*
	Adds a hook handler to the hook.

	@param type The hook type.
	@param pFunc The hook handler that should be added.
	*/
	void AddCallback(HookType_t type, HookHandlerFn* pFunc);
	
	/*
	Removes a hook handler to the hook.

	@param type The hook type.
	@param pFunc The hook handler that should be removed.
	*/
	void RemoveCallback(HookType_t type, HookHandlerFn* pFunc);
	
	/*
	Checks if a hook handler is already added.

	@param type The hook type.
	@param pFunc The hook handler that should be checked.
	*/
	bool IsCallbackRegistered(HookType_t type, HookHandlerFn* pFunc);
	
	
	/*
	Return the CRegisters pointer based on m_bUsePreRegister. It returns
	m_pRegisterPre in a pre-hook and m_pRegisterPost in a post-hook.
	*/
	CRegisters* GetRegisters();

	template<class T>
	T GetArgument(int iIndex)
	{
		return *(T *) m_pCallingConvention->GetArgumentPtr(iIndex, GetRegisters());
	}

	template<class T>
	void SetArgument(int iIndex, T value)
	{
		CRegisters* pRegisters = GetRegisters();
		void* pPtr = m_pCallingConvention->GetArgumentPtr(iIndex, pRegisters);
		*(T *) pPtr = value;
		m_pCallingConvention->ArgumentPtrChanged(iIndex, pRegisters, pPtr);
	}

	template<class T>
	T GetReturnValue()
	{
		return *(T *)  m_pCallingConvention->GetReturnPtr(GetRegisters());
	}

	template<class T>
	void SetReturnValue(T value)
	{
		CRegisters* pRegisters = GetRegisters();
		void* pPtr = m_pCallingConvention->GetReturnPtr(pRegisters);
		*(T *)  pPtr = value;
		m_pCallingConvention->ReturnPtrChanged(pRegisters, pPtr);
	}

private:
	bool CreateBridge();

	void Write_ModifyReturnAddress(CHookAssembler& assembler);
	void Write_CallHandler(CHookAssembler& assembler, HookType_t type, CRegisters* pRegisters);
	// bWithST0: whether to save/restore st0, which only holds a value (the return value) after
	// the function has returned. At function entry the x87 stack is empty.
	void Write_SaveRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0);
	void Write_RestoreRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0);
	void Write_LoadST0(CHookAssembler& assembler, CRegisters* pRegisters);

	bool CreatePostCallback();

	bool __cdecl HookHandler(HookType_t type);

	void* __cdecl GetReturnAddress(void* pESP);
	void __cdecl SetReturnAddress(void* pRetAddr, void* pESP);

public:
	std::map<HookType_t, std::list<HookHandlerFn*> > m_hookHandler;

	// Address of the original function
	void* m_pFunc;

	ICallingConvention* m_pCallingConvention;

	// Address of the bridge
	void* m_pBridge;

	// Address of the trampoline
	void* m_pTrampoline;

	// Register storage
	CRegisters* m_pRegistersPre;
	CRegisters* m_pRegistersPost;

	// New return address
	void* m_pNewRetAddr;

	std::map<void*, std::vector<void*> > m_RetAddr;

	bool m_bUsePreRegisters;

	// The bytes of the function the hook's jump replaced, restored when unhooking.
	std::vector<unsigned char> m_OriginalBytes;

	// AsmJit's runtime (owns m_pTrampoline, m_pBridge and m_pNewRetAddr)
	CHookJit* m_pJit;
};

#endif // _HOOK_H