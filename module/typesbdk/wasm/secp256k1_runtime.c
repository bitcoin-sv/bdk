/******************************************************************************
 * libsecp256k1 verifier with runtime-provided fixed-base tables.
 ******************************************************************************/

/* Prepare the internal types before suppressing the static table header. */
#define SECP256K1_BUILD
#include "secp256k1.h"
#include "assumptions.h"
#include "checkmem.h"
#include "util.h"
#include "field_impl.h"
#include "scalar_impl.h"
#include "group_impl.h"
#include "ecmult.h"
#include "secp256k1_runtime_precomputed.h"

/* Reuse the curve endomorphism for the fixed generator as well as arbitrary
 * public keys. This lets the second W15 lane share the first lane's points,
 * transformed once during startup instead of independently generated. */
#define secp256k1_scalar_split_128 secp256k1_scalar_split_lambda

/* Compile the upstream implementation unchanged after replacing only its
 * deterministic fixed-base table declarations. */
#include "secp256k1.c"
