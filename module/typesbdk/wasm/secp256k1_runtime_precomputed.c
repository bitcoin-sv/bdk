/******************************************************************************
 * Runtime construction of libsecp256k1's deterministic verification tables.
 *
 * THIS FILE REPLACES A NATIVE libsecp256k1 TABLE. Upstream's secp256k1_precomputed
 * object library compiles two generated sources (src/secp256k1/src/CMakeLists.txt):
 *
 *   precomputed_ecmult.c      ~2.3 MB  the large fixed-base VERIFICATION table
 *   precomputed_ecmult_gen.c  ~260 KB  the compact SIGNING generator table
 *
 * module/typesbdk/wasm/CMakeLists.txt overwrites that target's SOURCES property,
 * listing only this file plus precomputed_ecmult_gen.c -- so precomputed_ecmult.c
 * is never compiled. The signing table stays static because it is already small.
 *
 * The definitions of secp256k1_pre_g and secp256k1_pre_g_128 below deliberately
 * carry the SAME symbol names upstream's dropped file would have defined, so the
 * linker still resolves every reference from ecmult_impl.h. The difference is that
 * upstream emits them as initialised const data, whereas here they are ordinary
 * zero-initialised globals filled in at run time. That converts ~2.3 MB of table
 * bytes into empty .bss plus the small generator below -- the reason the shipped
 * WASM payload fits its size budget.
 *
 * CONSEQUENCE: the tables are ALL ZERO until
 * bdk_secp256k1_prepare_verification_tables() runs. Every binding entry point in
 * txvalidator_wasm.cpp calls it before touching the curve. Any other consumer that
 * links these targets -- notably libsecp256k1's own tests/noverify_tests binaries,
 * which link secp256k1_precomputed -- will compute wrong results unless it calls
 * that function first. Those upstream suites are therefore not enabled for this
 * module; see the rationale in module/typesbdk/wasm/CMakeLists.txt.
 ******************************************************************************/

#include "field_impl.h"
#include "group_impl.h"
#include "ecmult.h"
#include "secp256k1_runtime_precomputed.h"

#include <stdlib.h>
#include <string.h>

int bdk_secp256k1_verification_snapshot_is_valid(
    const unsigned char* input,
    size_t size);

secp256k1_ge_storage secp256k1_pre_g[ECMULT_TABLE_SIZE(WINDOW_G)];
secp256k1_ge_storage secp256k1_pre_g_128[ECMULT_TABLE_SIZE(WINDOW_G)];
static int verification_tables_prepared = 0;

/* Generate odd multiples with one field inversion for the complete table.
 * The upstream table generator converts every point independently because it
 * only runs at build time. That is needlessly expensive on first verification. */
static void bdk_secp256k1_compute_table(
    secp256k1_ge_storage* table,
    secp256k1_ge_storage* lambda_table,
    const secp256k1_gej* generator
) {
    const size_t count = ECMULT_TABLE_SIZE(WINDOW_G);
    secp256k1_ge* points = malloc(count * sizeof(*points));
    secp256k1_fe* ratios = malloc(count * sizeof(*ratios));
    secp256k1_gej doubled_generator;
    secp256k1_gej accumulator;
    secp256k1_ge doubled_generator_isomorphic;
    secp256k1_fe global_z;
    secp256k1_fe inverse_z;
    size_t i;

    if (points == NULL || ratios == NULL) {
        abort();
    }

    /* Match libsecp256k1's optimized odd-multiple construction. Treat the
     * doubled generator as affine on an isomorphic curve, avoiding any field
     * inversion until the complete table is normalized. */
    secp256k1_gej_double_var(&doubled_generator, generator, NULL);
    secp256k1_ge_set_xy(
        &doubled_generator_isomorphic,
        &doubled_generator.x,
        &doubled_generator.y
    );
    secp256k1_ge_set_gej_zinv(&points[0], generator, &doubled_generator.z);
    secp256k1_gej_set_ge(&accumulator, &points[0]);
    accumulator.z = generator->z;
    ratios[0] = doubled_generator.z;

    for (i = 1; i < count; ++i) {
        secp256k1_gej_add_ge_var(
            &accumulator,
            &accumulator,
            &doubled_generator_isomorphic,
            &ratios[i]
        );
        secp256k1_ge_set_xy(&points[i], &accumulator.x, &accumulator.y);
    }

    secp256k1_fe_mul(&global_z, &accumulator.z, &doubled_generator.z);
    secp256k1_ge_table_set_globalz(count, points, ratios);
    secp256k1_fe_inv_var(&inverse_z, &global_z);
    for (i = 0; i < count; ++i) {
        secp256k1_fe lambda_x;
        secp256k1_ge_set_ge_zinv(&points[i], &points[i], &inverse_z);
        secp256k1_ge_to_storage(&table[i], &points[i]);
        lambda_x = points[i].x;
        secp256k1_fe_mul(&lambda_x, &lambda_x, &secp256k1_const_beta);
        secp256k1_fe_to_storage(&lambda_table[i].x, &lambda_x);
        lambda_table[i].y = table[i].y;
    }

    free(ratios);
    free(points);
}

void bdk_secp256k1_prepare_verification_tables(void) {
    secp256k1_gej generator;

    if (verification_tables_prepared) {
        return;
    }
    secp256k1_gej_set_ge(&generator, &secp256k1_ge_const_g);
    bdk_secp256k1_compute_table(
        secp256k1_pre_g,
        secp256k1_pre_g_128,
        &generator
    );
    verification_tables_prepared = 1;
}

size_t bdk_secp256k1_verification_table_snapshot_size(void) {
    return sizeof(secp256k1_pre_g) + sizeof(secp256k1_pre_g_128);
}

int bdk_secp256k1_export_verification_tables(unsigned char* output, size_t size) {
    const size_t first_size = sizeof(secp256k1_pre_g);
    if (output == NULL || size != bdk_secp256k1_verification_table_snapshot_size()) {
        return 0;
    }
    bdk_secp256k1_prepare_verification_tables();
    memcpy(output, secp256k1_pre_g, first_size);
    memcpy(output + first_size, secp256k1_pre_g_128, sizeof(secp256k1_pre_g_128));
    return 1;
}

int bdk_secp256k1_import_verification_tables(const unsigned char* input, size_t size) {
    const size_t first_size = sizeof(secp256k1_pre_g);
    if (input == NULL ||
        size != bdk_secp256k1_verification_table_snapshot_size() ||
        !bdk_secp256k1_verification_snapshot_is_valid(input, size)) {
        return 0;
    }
    memcpy(secp256k1_pre_g, input, first_size);
    memcpy(secp256k1_pre_g_128, input + first_size, sizeof(secp256k1_pre_g_128));
    verification_tables_prepared = 1;
    return 1;
}
