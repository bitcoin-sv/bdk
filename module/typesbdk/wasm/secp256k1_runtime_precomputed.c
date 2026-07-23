/******************************************************************************
 * Runtime construction of libsecp256k1's deterministic verification tables.
 ******************************************************************************/

#include "field_impl.h"
#include "group_impl.h"
#include "ecmult.h"
#include "secp256k1_runtime_precomputed.h"

#include <stdlib.h>

secp256k1_ge_storage secp256k1_pre_g[ECMULT_TABLE_SIZE(WINDOW_G)];
secp256k1_ge_storage secp256k1_pre_g_128[ECMULT_TABLE_SIZE(WINDOW_G)];

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
    static int prepared = 0;
    secp256k1_gej generator;

    if (prepared) {
        return;
    }
    secp256k1_gej_set_ge(&generator, &secp256k1_ge_const_g);
    bdk_secp256k1_compute_table(
        secp256k1_pre_g,
        secp256k1_pre_g_128,
        &generator
    );
    prepared = 1;
}
