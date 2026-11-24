# DynamicHooks

A library for creating dynamic function hooks (pre- and post-hooks) on x86, for the calling
conventions of 32-bit MSVC (cdecl, thiscall, stdcall, fastcall) and GCC (cdecl, thiscall), and of
x86-64 (System V on Linux, Microsoft's on Windows). The argument and return types of a hooked
function are described at runtime; the code that saves the registers and calls the hook
handlers is generated with [AsmJit](https://asmjit.com).

Used by [Source.Python](https://github.com/pet0ruk/Source.Python), which builds it with this
repository's CMake (`add_subdirectory`, providing its own `asmjit::asmjit` target). Forked from
[Ayuto/DynamicHooks](https://github.com/Ayuto/DynamicHooks).

## 32-bit and x86-64

A build is for one architecture, chosen with the `DYNAMICHOOKS_X86_64` option (with MSVC it
follows `-A`; the build fails if it doesn't match the compiler's target):

- **32-bit x86**: the conventions in `conventions/x86*.h`. The hook replaces the function's first
  instructions with a jump and moves them to a trampoline it builds itself (`trampoline.h`).
- **x86-64**: `conventions/x86_64SystemVDefault.h` (Linux) and
  `conventions/x86_64MicrosoftDefault.h` (Windows), for arguments and return values of the types
  in `DataType_t` (integers, pointers, float, double; not structures passed by value). The
  function is patched with [safetyhook](https://github.com/cursey/safetyhook), which builds the
  trampoline and relocates the instructions it moves, RIP-relative ones included. The Microsoft
  convention follows the design of SourceMod's DynamicHooks-x86_64.

`Register_t` has the x86-64 registers (RAX to R15, XMM8 to XMM15) after the 32-bit ones, so those
keep their values.

## Building

Needs CMake 3.19 or later and network access (AsmJit is downloaded, pinned by commit and
sha256 in `cmake/asmjit.cmake`; for x86-64 also safetyhook's amalgamated release, with the Zydis
disassembler, pinned in `cmake/safetyhook.cmake`). safetyhook needs C++23; DynamicHooks' headers
don't include it, so code using DynamicHooks doesn't.

```bash
# Linux 32-bit (-m32 is added; a 64-bit host needs gcc-multilib/g++-multilib)
cmake -S . -B build && cmake --build build && ctest --test-dir build

# Linux x86-64
cmake -S . -B build -DDYNAMICHOOKS_X86_64=ON && cmake --build build && ctest --test-dir build

# Windows (Visual Studio): -A Win32 or -A x64
cmake -S . -B build -A Win32
cmake --build build --config Release && ctest --test-dir build -C Release
```

The tests (`tests/`, option `DYNAMICHOOKS_TESTS`, on when built on its own) hook a function of
each calling convention and check the arguments and return values the hooks see and set.
`tests/conventions.cpp` is built for every 32-bit convention and return type (int, long long,
float, double), with arguments of each size; it also checks that a pre-hook can skip the
function, that unhooking restores it, and that the stack pointer and x87 stack are as the caller
left them. On x86-64, `tests/x86_64.cpp` is built for each return type (int, long long, float,
double, pointer): a function with arguments in general purpose and XMM registers and on the
stack, a member function, recursion, a 16-byte aligned stack in the handlers, and a function
starting with a RIP-relative instruction. CI (`.github/workflows/ci.yml`) runs them on Linux
i686 and x86_64 and Windows Win32 and x64.

## Licence

zlib (see the header of each source file). safetyhook is under the Boost Software License 1.0,
Zydis under the MIT licence; both are downloaded at build time, not part of this repository.
