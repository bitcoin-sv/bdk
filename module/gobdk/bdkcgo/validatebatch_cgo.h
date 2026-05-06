#ifndef __VALIDATEBATCH_CGO_H__
#define __VALIDATEBATCH_CGO_H__

#include <stdint.h>
#include <stdbool.h>

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
 * ValidateBatch_Create allocates a new ValidateBatch object on the heap
 * ValidateBatch_Destroy deallocates the ValidateBatch object
 *
 * Note: Destroy is necessary because Create uses 'new' to allocate the object.
 * Clear() only empties the internal vector, but doesn't free the ValidateBatch itself.
 */
ValidateBatchCGO ValidateBatch_Create();
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
 */
void ValidateBatch_Add(
    ValidateBatchCGO cgoBatch,
    const char* extendedTxPtr, int extendedTxLen,
    const int32_t* hUTXOsPtr, int hUTXOsLen,
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
int ValidateBatch_Size(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Empty checks if the batch is empty
 * Returns true if the batch contains no elements
 */
bool ValidateBatch_Empty(ValidateBatchCGO cgoBatch);

/**
 * ValidateBatch_Reserve pre-allocates capacity for the specified number of elements
 * This is an optimization to avoid multiple reallocations when batch size is known
 */
void ValidateBatch_Reserve(ValidateBatchCGO cgoBatch, int capacity);

#ifdef __cplusplus
}
#endif

#endif /* __VALIDATEBATCH_CGO_H__ */
