// Builds bitcoin-sv's SHA-NI transforms (src/crypto/sha256d64_shani.cpp, from
// Bitcoin Core) for x86-64 targets only.
//
// The SHA, SSE4.1 and SSSE3 features come from a target pragma around the
// included source, not from per-file compiler flags, so they follow the
// compiler's target rather than the build machine: a macOS universal, Rosetta
// or cross build gets these transforms in its x86-64 code and nowhere else.
// Elsewhere (ARM, WASM, MSVC/clang-cl) this is an empty translation unit.

#if (defined(__x86_64__) || defined(__amd64__)) && (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER) && !defined(__EMSCRIPTEN__)

// The included source's own headers, first, so their inline functions keep the
// default target
#include <attributes.h>

#define ENABLE_X86_SHANI 1 // NOLINT(cppcoreguidelines-macro-usage)

#if defined(__clang__)
#pragma clang attribute push(__attribute__((target("sha,sse4.1,ssse3"))), apply_to = function)
#else
#pragma GCC push_options
#pragma GCC target("sha,sse4.1,ssse3")
#endif

// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "crypto/sha256d64_shani.cpp"

#if defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif

#endif
