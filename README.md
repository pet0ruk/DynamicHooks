# DynamicHooks

A library for creating dynamic function hooks (pre- and post-hooks) on 32-bit x86, for the
calling conventions of MSVC (cdecl, thiscall, stdcall, fastcall) and GCC (cdecl, thiscall). The
argument and return types of a hooked function are described at runtime; the code that saves
the registers and calls the hook handlers is generated with [AsmJit](https://asmjit.com).

Used by [Source.Python](https://github.com/pet0ruk/Source.Python), which builds it with this
repository's CMake (`add_subdirectory`, providing its own `asmjit::asmjit` target). Forked from
[Ayuto/DynamicHooks](https://github.com/Ayuto/DynamicHooks).

## Building

Needs CMake 3.19 or later and network access (AsmJit is downloaded, pinned by commit and
sha256 in `cmake/asmjit.cmake`).

```bash
# Linux (32-bit: -m32 is added; a 64-bit host needs gcc-multilib/g++-multilib)
cmake -S . -B build && cmake --build build && ctest --test-dir build

# Windows (Visual Studio)
cmake -S . -B build -A Win32
cmake --build build --config Release && ctest --test-dir build -C Release
```

The tests (`tests/`, option `DYNAMICHOOKS_TESTS`, on when built on its own) hook a function of
each calling convention and check the arguments and return values the hooks see and set.
`tests/conventions.cpp` is built for every convention and return type (int, long long, float,
double), with arguments of each size; it also checks that a pre-hook can skip the function,
that unhooking restores it, and that the stack pointer and x87 stack are as the caller left
them. CI (`.github/workflows/ci.yml`) runs them on Linux i686 and Windows Win32.

## Licence

zlib (see the header of each source file).
