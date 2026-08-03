/******************************************************************************
 * wasm_tests.c -- run libsecp256k1's own unit suite against the WASM module's
 * runtime-reconstructed verification tables (secp256k1_pre_g / secp256k1_pre_g_128).
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * The WASM build replaces upstream's static const fixed-base tables with
 * runtime-computed ones and fills the second table with the lambda-endomorphism
 * decomposition instead of odd multiples of 2^128*G (see secp256k1_runtime.c and
 * secp256k1_runtime_precomputed.c). Linked as-is, upstream's tests.c fails two ways:
 *
 *   (a) the tables are all zero until bdk_secp256k1_prepare_verification_tables()
 *       runs, and only the module's bindings call it; and
 *   (b) tests.c compiles its own secp256k1.c, whose ecmult uses the plain 2^128
 *       scalar split (secp256k1_scalar_split_128), which does not match the
 *       lambda-filled secp256k1_pre_g_128.
 *
 * This wrapper fixes both, mirroring secp256k1_runtime.c's include ordering: the
 * implementation headers are pre-included first (so scalar_split_128 and
 * scalar_split_lambda are both defined once, unmangled, and the upstream table
 * header is suppressed), then the scalar_split_128 -> scalar_split_lambda
 * redefinition is installed and the *patched* tests.c is pulled in (b). Because
 * ecmult_impl.h is deliberately NOT pre-included here, its scalar_split_128 call
 * site is the one macro-rewritten. Breakage (a) is handled inside the patched
 * tests.c, whose main() calls bdk_secp256k1_prepare_verification_tables() as its
 * first statement (wasm_tests_patch.cmake) -- an explicit call rather than a
 * constructor, so link-time optimisation cannot reorder or elide the priming.
 *
 * A passing run therefore validates the exact verification tables the shipped
 * bdk-core artifacts use. This executable is build-tree only; it is never
 * published and never one of the committed artifacts.
 ******************************************************************************/

/* Prepare the internal types before suppressing the static table header. */
#define SECP256K1_BUILD
#include "secp256k1.h"
#include "assumptions.h"
#include "checkmem.h"
#include "util.h"
#include "field_impl.h"
#include "scalar_impl.h"                 /* defines split_128 AND split_lambda */
#include "group_impl.h"
#include "ecmult.h"                      /* declaration header only -- NOT ecmult_impl.h */
#include "secp256k1_runtime_precomputed.h" /* suppresses precomputed_ecmult.h; declares the tables */

/* (a) is handled by the patched tests.c main() calling
 * bdk_secp256k1_prepare_verification_tables() first (see wasm_tests_patch.cmake);
 * the declaration comes from secp256k1_runtime_precomputed.h above. */

/* Capture the genuine 128-bit split before the macro below, so the patched suite
 * can keep test_fixed_wnaf() exercising split_128 rather than the endomorphism
 * split. The patch script rewrites the tests.c call site to call this alias. */
static void bdk_wasm_split_128(secp256k1_scalar *r1, secp256k1_scalar *r2,
                               const secp256k1_scalar *k) {
    secp256k1_scalar_split_128(r1, r2, k);
}

/* (b) Match the shipped module: the ecmult generator lane uses the lambda split. */
#define secp256k1_scalar_split_128 secp256k1_scalar_split_lambda

/* Resolves to the build-tree patched copy (wasm_tests_patch.cmake); its
 * #include "secp256k1.c" pulls in ecmult_impl.h, whose split call site is now
 * rewritten to split_lambda. */
#include "tests.c"
