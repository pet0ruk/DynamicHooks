# =============================================================================
# AsmJit, pinned by commit and the sha256 of GitHub's archive of it, built as a static library
# (asmjit::asmjit). To build against a local checkout instead:
#   -DFETCHCONTENT_SOURCE_DIR_ASMJIT=<path>
# =============================================================================
include(FetchContent)

set(DYNAMICHOOKS_ASMJIT_COMMIT 677a10bac2c24f7e068cf1f9e675bf51c7b534ea)   # 1.25.0 (master, 2026-11-04)
set(DYNAMICHOOKS_ASMJIT_SHA256 0bcc7d502846a307fa72c0fc0bf96a2f1b586a9f4706fe98e03cbb0852cda73c)

FetchContent_Declare(asmjit
    URL https://github.com/asmjit/asmjit/archive/${DYNAMICHOOKS_ASMJIT_COMMIT}.tar.gz
    URL_HASH SHA256=${DYNAMICHOOKS_ASMJIT_SHA256}
)
set(ASMJIT_STATIC     TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_NATVIS  TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_INSTALL TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(asmjit)
set_target_properties(asmjit PROPERTIES POSITION_INDEPENDENT_CODE ON)
