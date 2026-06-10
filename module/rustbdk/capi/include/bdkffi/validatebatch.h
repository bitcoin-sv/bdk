#ifndef BDKFFI_VALIDATEBATCH_H
#define BDKFFI_VALIDATEBATCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* bdkffi_validatebatch_t;

bdkffi_validatebatch_t bdkffi_validatebatch_create(void);
void bdkffi_validatebatch_destroy(bdkffi_validatebatch_t batch);

/*
 * Adds a validation argument using non-owning spans over caller-provided memory.
 * The caller must keep extended_tx and utxo_heights alive until the batch is
 * cleared, destroyed, or consumed by bdkffi_txvalidator_validate_batch.
 */
void bdkffi_validatebatch_add(
    bdkffi_validatebatch_t batch,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus);

void bdkffi_validatebatch_clear(bdkffi_validatebatch_t batch);
int bdkffi_validatebatch_size(bdkffi_validatebatch_t batch);
bool bdkffi_validatebatch_empty(bdkffi_validatebatch_t batch);
void bdkffi_validatebatch_reserve(bdkffi_validatebatch_t batch, int capacity);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_VALIDATEBATCH_H */
