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
#include <map>
#include <memory>
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

/*
A hook handler. A pre-hook handler that returns true skips the original function (the hook
then returns the return value the handlers set).
*/
typedef bool (*HookHandlerFn)(HookType_t, CHook*);

struct CHookJit;
struct CHookAssembler;

#ifdef __linux__
#define __cdecl
#endif


// ============================================================================
// >> CHook
// ============================================================================
/*
A hooked function. Created and owned by CHookManager.
*/
class CHook
{
private:
	friend class CHookManager;

	/*
	@param <pFunc>: The address of the function to hook.
	@param <pConvention>: The calling convention of <pFunc>.
	*/
	CHook(void* pFunc, std::shared_ptr<ICallingConvention> pConvention);

	/*
	Patches the function to jump to the hook. Returns false, leaving the function unchanged, if
	its start can't be moved to a trampoline (see BuildTrampoline in trampoline.h) or memory for
	the hook can't be allocated or written.
	*/
	bool Install();

public:
	// Restores the function (if Install() patched it) and frees the hook's code.
	~CHook();

	CHook(const CHook&) = delete;
	CHook& operator=(const CHook&) = delete;

	/*
	Adds a hook handler. A handler is only added once per type.
	*/
	void AddCallback(HookType_t eHookType, HookHandlerFn pCallback);

	/*
	Removes a hook handler.
	*/
	void RemoveCallback(HookType_t eHookType, HookHandlerFn pCallback);

	/*
	Returns whether a hook handler was added.
	*/
	bool IsCallbackRegistered(HookType_t eHookType, HookHandlerFn pCallback) const;

	// The address of the hooked function.
	void* GetFunction() const
	{
		return m_pFunc;
	}

	// The address of the trampoline: calling it calls the original function without the hook.
	void* GetTrampoline() const
	{
		return m_pTrampoline;
	}

	ICallingConvention* GetCallingConvention() const
	{
		return m_pCallingConvention.get();
	}

	/*
	The registers saved for the handlers running now: those saved before the function in a
	pre-hook, those saved after it in a post-hook.
	*/
	CRegisters* GetRegisters();

	/*
	Whether GetRegisters returns the registers saved before the function (true in a pre-hook)
	or after it (true in a post-hook: false). A post-hook handler can set it to read the
	arguments as they were passed.
	*/
	bool GetUsePreRegisters() const
	{
		return m_bUsePreRegisters;
	}

	void SetUsePreRegisters(bool bUsePreRegisters)
	{
		m_bUsePreRegisters = bUsePreRegisters;
	}

	/*
	The return address of the innermost call in progress whose stack pointer (at the function's
	entry) was <pESP>, or nullptr.
	*/
	void* FindReturnAddress(void* pESP) const;

	template<class T>
	T GetArgument(int iIndex)
	{
		return *static_cast<T*>(m_pCallingConvention->GetArgumentPtr(iIndex, GetRegisters()));
	}

	template<class T>
	void SetArgument(int iIndex, T value)
	{
		CRegisters* pRegisters = GetRegisters();
		void* pPtr = m_pCallingConvention->GetArgumentPtr(iIndex, pRegisters);
		*static_cast<T*>(pPtr) = value;
		m_pCallingConvention->ArgumentPtrChanged(iIndex, pRegisters, pPtr);
	}

	template<class T>
	T GetReturnValue()
	{
		return *static_cast<T*>(m_pCallingConvention->GetReturnPtr(GetRegisters()));
	}

	template<class T>
	void SetReturnValue(T value)
	{
		CRegisters* pRegisters = GetRegisters();
		void* pPtr = m_pCallingConvention->GetReturnPtr(pRegisters);
		*static_cast<T*>(pPtr) = value;
		m_pCallingConvention->ReturnPtrChanged(pRegisters, pPtr);
	}

private:
	bool CreateBridge();
	bool CreatePostCallback();

	void Write_ModifyReturnAddress(CHookAssembler& assembler);
	void Write_CallHandler(CHookAssembler& assembler, HookType_t type, CRegisters* pRegisters);
	void Write_SaveRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0);
	void Write_RestoreRegisters(CHookAssembler& assembler, CRegisters* pRegisters, bool bWithST0);
	void Write_LoadST0(CHookAssembler& assembler, CRegisters* pRegisters);

	// Called by the hook's code (cdecl, with the hook as the first argument).
	static bool __cdecl HookHandler(CHook* pHook, HookType_t eHookType);
	static void* __cdecl PopReturnAddress(CHook* pHook, void* pESP);
	static void __cdecl PushReturnAddress(CHook* pHook, void* pRetAddr, void* pESP);

private:
	void* m_pFunc;
	std::shared_ptr<ICallingConvention> m_pCallingConvention;

	// The hook handlers, per HookType_t.
	std::vector<HookHandlerFn> m_Handlers[2];

	// AsmJit's runtime (owns m_pTrampoline, m_pBridge and m_pNewRetAddr).
	std::unique_ptr<CHookJit> m_pJit;

	// The code the function jumps to, and the post-hook code the function returns to.
	void* m_pBridge;
	void* m_pNewRetAddr;

	// The function's first instructions, followed by a jump to the rest of the function.
	void* m_pTrampoline;

	// The bytes of the function the hook's jump replaced, restored when unhooking.
	std::vector<unsigned char> m_OriginalBytes;

	std::unique_ptr<CRegisters> m_pRegistersPre;
	std::unique_ptr<CRegisters> m_pRegistersPost;
	bool m_bUsePreRegisters;

	// The original return addresses of the calls in progress, by their stack pointer.
	std::map<void*, std::vector<void*> > m_RetAddr;
};

#endif // _HOOK_H
