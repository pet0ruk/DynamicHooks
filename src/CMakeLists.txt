# =============================================================================
# The DynamicHooks library (static). It needs the asmjit::asmjit target (see ../CMakeLists.txt).
# =============================================================================
add_library(DynamicHooks STATIC
    asm.cpp
    hook.cpp
    manager.cpp
    registers.cpp
    trampoline.cpp
    utilities.cpp
    conventions/x86Convention.cpp
    conventions/x86MsCdecl.cpp
    conventions/x86MsThiscall.cpp
    conventions/x86MsStdcall.cpp
    conventions/x86MsFastcall.cpp
)

# Users include "hook.h", "conventions/x86MsCdecl.h", ...; none of the headers needs AsmJit, so
# it is linked privately (code using DynamicHooks gets neither its headers nor its C++17 needs).
target_include_directories(DynamicHooks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DynamicHooks PRIVATE asmjit::asmjit)
set_target_properties(DynamicHooks PROPERTIES POSITION_INDEPENDENT_CODE ON)
