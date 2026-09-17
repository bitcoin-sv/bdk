#ifndef __VALIDATEBATCH_CGO_H__
#define __VALIDATEBATCH_CGO_H__

#include <stdint.h>
#include <stdbool.h>
#include <bdkcgo/txerror_cgo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ValidateBatchCGO implements the CGO handler for ValidateBatch class
 * This opaque pointer allows C/CGO code to manage ValidateBatch instances
 */
typedef void* ValidateBatchCGO;

/**
 * Handle constructor and destructor
 *
 * ValidateBatch_CreateV2 allocates a new ValidateBatch object on the heap
 * ValidateBatch_Destroy deallocates the ValidateBatch object
 *
 * Note: Destroy is necessary because Create uses 'new' to allocate the object.
 * Clear() only empties the internal vector, but doesn't free the ValidateBatch itself.
 *
 * The V2 suffix is this surface's ABI generation marker. Its own signature is
 * unchanged, but it is the only source of the ValidateBatchCGO handle that every
 * changed call below requires, so a consumer built against an archive predating the
 * uint64_t length ABI fails to link here instead of silently reading a narrowed
 * length. This surface is reachable without ever constructing a TxValidator, which
 * is why it carries its own marker.
 */
ValidateBatchCGO ValidateBatch_CreateV2();
void ValidateBatch_Destroy(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Add adds a validation argument to the batch using C-style parameters
 *
 * Parameters:
 *   cgoBatch - The batch to add to
 *   extendedTxPtr - Pointer to extended transaction binary data
 *   extendedTxLen - Length of extended transaction data
 *   hUTXOsPtr - Pointer to array of UTXO heights
 *   hUTXOsLen - Number of elements in UTXO heights array
 *   blockHeight - Block height for validation
 *   consensus - Consensus flag
 *
 * Returns { TX_ERR_DOMAIN_OK, 0 } when the entry was appended.
 * Returns { TX_ERR_DOMAIN_ABI, AbiError_t } when an argument could not be expressed,
 * and { TX_ERR_DOMAIN_EXCEPTION, 0 } for any other failure. The entry is NOT appended
 * in either failure case: batch results are positional, so the caller must treat a
 * non-OK return as a reason to abandon the batch rather than continue filling it.
 */
TxError ValidateBatch_Add(
    ValidateBatchCGO cgoBatch,
    const char* extendedTxPtr, uint64_t extendedTxLen,
    const int32_t* hUTXOsPtr, uint64_t hUTXOsLen,
    int32_t blockHeight,
    bool consensus
);

/**
 * ValidateBatch_Clear removes all elements from the batch
 * The batch can be reused after clearing
 */
void ValidateBatch_Clear(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Size returns the number of elements in the batch
 */
uint64_t ValidateBatch_Size(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Empty checks if the batch is empty
 * Returns true if the batch contains no elements
 */
bool ValidateBatch_Empty(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Reserve pre-allocates capacity for the specified number of elements
 * This is an optimization to avoid multiple reallocations when batch size is known
 *
 * Reserving is best effort and never fails: a capacity this platform or the container
 * cannot hold is ignored, and an allocation failure is swallowed locally. Honouring
 * the hint is optional, letting an exception cross this boundary is not.
 */
void ValidateBatch_Reserve(ValidateBatchCGO cgoBatch, uint64_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* __VALIDATEBATCH_CGO_H__ */
