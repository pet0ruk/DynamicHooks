# =============================================================================
# safetyhook (x86-64 builds only): patches the hooked function and builds its trampoline,
# relocating the instructions it moves (including RIP-relative ones). Its amalgamated release,
# safetyhook.cpp/.hpp with Zydis.c/.h (the disassembler it uses), pinned by version and the
# sha256 of the release asset, built as a static library (safetyhook::safetyhook).
# safetyhook needs C++23 (std::expected); only DynamicHooks' .cpp files include it.
# To build against a local copy of the unpacked asset instead:
#   -DFETCHCONTENT_SOURCE_DIR_SAFETYHOOK=<path>
# =============================================================================
include(FetchContent)

set(DYNAMICHOOKS_SAFETYHOOK_VERSION 0.7.0)
set(DYNAMICHOOKS_SAFETYHOOK_SHA256 505d4c07ec1c5b94a17f3906ca86afbe1264e738d8becaa244866694c6200c2c)

FetchContent_Declare(safetyhook
    URL https://github.com/cursey/safetyhook/releases/download/v${DYNAMICHOOKS_SAFETYHOOK_VERSION}/safetyhook-amalgamated-zydis.zip
    URL_HASH SHA256=${DYNAMICHOOKS_SAFETYHOOK_SHA256}
)
FetchContent_MakeAvailable(safetyhook)

enable_language(C)
add_library(safetyhook STATIC
    ${safetyhook_SOURCE_DIR}/safetyhook.cpp
    ${safetyhook_SOURCE_DIR}/Zydis.c
)
add_library(safetyhook::safetyhook ALIAS safetyhook)
target_include_directories(safetyhook PUBLIC ${safetyhook_SOURCE_DIR})
# Zydis.h is amalgamated with its decoder only and linked statically.
target_compile_definitions(safetyhook PUBLIC ZYDIS_STATIC_BUILD ZYCORE_STATIC_BUILD)
target_compile_features(safetyhook PUBLIC cxx_std_23)
set_target_properties(safetyhook PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # Zydis.c is one 11 MB file
    target_compile_options(safetyhook PRIVATE /bigobj)
else()
    target_compile_options(safetyhook PRIVATE -w)
endif()
