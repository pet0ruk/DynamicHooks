# =============================================================================
# DynamicHooks: function hooking for 32-bit x86 and x86-64 (Windows: MSVC; Linux: GCC)
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build
#   (Windows: cmake -S . -B build -A Win32, then add --config Release to both)
#
#   x86-64: Linux cmake -S . -B build -DDYNAMICHOOKS_X86_64=ON; Windows cmake -S . -B build -A x64
#
# Built on its own, it fetches AsmJit (cmake/asmjit.cmake) and builds the tests (option
# DYNAMICHOOKS_TESTS). A project that adds it with add_subdirectory() can provide its own
# asmjit::asmjit target first; otherwise the pinned AsmJit is fetched the same way. x86-64 builds
# also fetch safetyhook (cmake/safetyhook.cmake).
#
# DYNAMICHOOKS_X86_64 says which architecture the compiler targets (with MSVC it follows -A; on
# Linux a top-level build adds -m32 unless it's ON, and a project adding DynamicHooks sets it to
# match its own flags). The build fails if it doesn't match the compiler's target.
# =============================================================================
cmake_minimum_required(VERSION 3.19)
project(DynamicHooks CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_top_level TRUE)
else()
    set(_top_level FALSE)
endif()
option(DYNAMICHOOKS_TESTS "Build the DynamicHooks tests" ${_top_level})
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_x86_64 ON)
else()
    set(_x86_64 OFF)
endif()
option(DYNAMICHOOKS_X86_64 "Build for x86-64 instead of 32-bit x86" ${_x86_64})

if(_top_level)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release)
    endif()
    if(MSVC)
        # the static C runtime, as Source.Python links it
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(NOT DYNAMICHOOKS_X86_64)
        add_compile_options(-m32)
        add_link_options(-m32)
    endif()
endif()

if(NOT TARGET asmjit::asmjit)
    include(cmake/asmjit.cmake)
endif()
if(DYNAMICHOOKS_X86_64 AND NOT TARGET safetyhook::safetyhook)
    include(cmake/safetyhook.cmake)
endif()

add_subdirectory(src)

if(DYNAMICHOOKS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
