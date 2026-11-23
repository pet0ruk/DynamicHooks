# =============================================================================
# AsmJit, pinned by commit and the sha256 of GitHub's archive of it, built as a static library
# (asmjit::asmjit). To build against a local checkout instead:
#   -DFETCHCONTENT_SOURCE_DIR_ASMJIT=<path>
# =============================================================================
include(FetchContent)

set(DYNAMICHOOKS_ASMJIT_COMMIT e8c8e2e48a1a38154c8e8864eb3bc61db80a1e31)   # 1.14.0, 2025-03-30
set(DYNAMICHOOKS_ASMJIT_SHA256 f3d0f56413dd88d069895e0f3933962cf1b687e901fb207a5b889ef5870b128a)

FetchContent_Declare(asmjit
    URL https://github.com/asmjit/asmjit/archive/${DYNAMICHOOKS_ASMJIT_COMMIT}.tar.gz
    URL_HASH SHA256=${DYNAMICHOOKS_ASMJIT_SHA256}
)
set(ASMJIT_STATIC     TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_NATVIS  TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_INSTALL TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(asmjit)
set_target_properties(asmjit PROPERTIES POSITION_INDEPENDENT_CODE ON)
