/******************************************************************************
 * Runtime verification-table declarations for size-constrained targets.
 ******************************************************************************/

#ifndef BDK_SECP256K1_RUNTIME_PRECOMPUTED_H
#define BDK_SECP256K1_RUNTIME_PRECOMPUTED_H

/* Suppress libsecp256k1's const, statically initialized declarations. */
#define SECP256K1_PRECOMPUTED_ECMULT_H

#define WINDOW_G ECMULT_WINDOW_SIZE

#ifdef __cplusplus
extern "C" {
#endif

extern secp256k1_ge_storage secp256k1_pre_g[ECMULT_TABLE_SIZE(WINDOW_G)];
extern secp256k1_ge_storage secp256k1_pre_g_128[ECMULT_TABLE_SIZE(WINDOW_G)];
void bdk_secp256k1_prepare_verification_tables(void);

#ifdef __cplusplus
}
#endif

#endif /* BDK_SECP256K1_RUNTIME_PRECOMPUTED_H */
