# =============================================================================
# Tests: each program hooks a function of one calling convention, checks the arguments and
# return values its pre- and post-hooks see and set, and exits non-zero (assert) on a mismatch.
# Run with ctest. The ms_* tests are for MSVC (Windows), the gcc_* ones for GCC (Linux).
# =============================================================================
if(MSVC)
    set(_tests ms_cdecl1 ms_cdecl2 ms_thiscall1 ms_thiscall2 ms_stdcall1 ms_stdcall2
               ms_fastcall1 ms_fastcall2)
else()
    set(_tests gcc_cdecl1 gcc_cdecl2 gcc_thiscall1 gcc_thiscall2)
endif()

foreach(_test ${_tests})
    add_executable(test_${_test} ${_test}.cpp)
    target_link_libraries(test_${_test} PRIVATE DynamicHooks)
    # Unoptimised, so the hooked functions stay real calls, and with assert() enabled in every
    # configuration (Release defines NDEBUG).
    if(MSVC)
        target_compile_options(test_${_test} PRIVATE /Od /Ob0 /UNDEBUG)
    else()
        target_compile_options(test_${_test} PRIVATE -O0 -UNDEBUG)
    endif()
    add_test(NAME ${_test} COMMAND test_${_test})
endforeach()
