// Builds the vendored Bitcoin Core ARMv8 SHA-2 transforms (sha256_arm_shani.cpp)
// for 64-bit ARM targets only.
//
// The SHA-2 instructions come from a target pragma around the included source,
// not from per-file compiler flags, so they follow the compiler's target rather
// than the build machine: a macOS universal, Rosetta or cross build gets these
// transforms in its arm64 code and nowhere else. Elsewhere (x86-64, WASM, MSVC/clang-cl)
// this is an empty translation unit. Whether the CPU can run them is decided at
// run time in sha256_backend.cpp.

#if defined(__aarch64__)                                                                 \
    && (defined(__GNUC__) || defined(__clang__))                                         \
    && !defined(_MSC_VER)                                                                \
    && !defined(__EMSCRIPTEN__)

// The included source's own headers, first, so their inline functions keep the
// default target
#include <array>
#include <cstddef>
#include <cstdint>

#if defined(__clang__)
#include <arm_acle.h>
#include <arm_neon.h>
#pragma clang attribute push(__attribute__((target("sha2"))), apply_to = function)
#else
// GCC may gate the crypto intrinsics on the target, so its headers come after the pragma
#pragma GCC push_options
#pragma GCC target("+crypto")
#include <arm_acle.h>
#include <arm_neon.h>
#endif

#define ENABLE_ARM_SHANI 1
#include "sha256_arm_shani.cpp"

#if defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif

#endif
