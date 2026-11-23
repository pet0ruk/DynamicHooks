# =============================================================================
# DynamicHooks: function hooking for 32-bit x86 (Windows: MSVC; Linux: GCC)
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build
#   (Windows: cmake -S . -B build -A Win32, then add --config Release to both)
#
# Built on its own, it fetches AsmJit (cmake/asmjit.cmake) and builds the tests (option
# DYNAMICHOOKS_TESTS). A project that adds it with add_subdirectory() can provide its own
# asmjit::asmjit target first; otherwise the pinned AsmJit is fetched the same way.
# =============================================================================
cmake_minimum_required(VERSION 3.19)
project(DynamicHooks CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_top_level TRUE)
else()
    set(_top_level FALSE)
endif()
option(DYNAMICHOOKS_TESTS "Build the DynamicHooks tests" ${_top_level})

if(_top_level)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release)
    endif()
    if(MSVC)
        # the static C runtime, as Source.Python links it
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        # the conventions are 32-bit x86 only
        add_compile_options(-m32)
        add_link_options(-m32)
    endif()
endif()

if(NOT TARGET asmjit::asmjit)
    include(cmake/asmjit.cmake)
endif()

add_subdirectory(src)

if(DYNAMICHOOKS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
