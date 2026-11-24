# =============================================================================
# The DynamicHooks library (static). It needs the asmjit::asmjit target and, for x86-64,
# safetyhook::safetyhook (see ../CMakeLists.txt).
# =============================================================================
add_library(DynamicHooks STATIC
    hook.cpp
    manager.cpp
    registers.cpp
    utilities.cpp
)
if(DYNAMICHOOKS_X86_64)
    target_sources(DynamicHooks PRIVATE
        hook_x64.cpp
        conventions/x86_64Convention.cpp
        conventions/x86_64SystemVDefault.cpp
        conventions/x86_64MicrosoftDefault.cpp
    )
else()
    target_sources(DynamicHooks PRIVATE
        asm.cpp
        hook_x86.cpp
        trampoline.cpp
        conventions/x86Convention.cpp
        conventions/x86MsCdecl.cpp
        conventions/x86MsThiscall.cpp
        conventions/x86MsStdcall.cpp
        conventions/x86MsFastcall.cpp
    )
endif()

# Users include "hook.h", "conventions/x86MsCdecl.h", ...; none of the headers needs AsmJit or
# safetyhook, so they are linked privately (code using DynamicHooks gets neither their headers
# nor their C++17/C++23 needs).
target_include_directories(DynamicHooks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DynamicHooks PRIVATE asmjit::asmjit)
if(DYNAMICHOOKS_X86_64)
    target_link_libraries(DynamicHooks PRIVATE safetyhook::safetyhook)
endif()
# Checked against the compiler's target in registers.cpp.
target_compile_definitions(DynamicHooks PRIVATE
    DYNAMICHOOKS_BUILD_X86_64=$<BOOL:${DYNAMICHOOKS_X86_64}>)
set_target_properties(DynamicHooks PROPERTIES POSITION_INDEPENDENT_CODE ON)
