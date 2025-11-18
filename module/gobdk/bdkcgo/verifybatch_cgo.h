#ifndef __VERIFYBATCH_CGO_H__
#define __VERIFYBATCH_CGO_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * VerifyBatchCGO implements the CGO handler for VerifyBatch class
 * This opaque pointer allows C/CGO code to manage VerifyBatch instances
 */
typedef void* VerifyBatchCGO;

/**
 * Handle constructor and destructor
 *
 * VerifyBatch_Create allocates a new VerifyBatch object on the heap
 * VerifyBatch_Destroy deallocates the VerifyBatch object
 *
 * Note: Destroy is necessary because Create uses 'new' to allocate the object.
 * Clear() only empties the internal vector, but doesn't free the VerifyBatch itself.
 */
VerifyBatchCGO VerifyBatch_Create();
void VerifyBatch_Destroy(VerifyBatchCGO cgoBatch);

/**
 * VerifyBatch_Add adds a verification argument to the batch using C-style parameters
 * This follows the same pattern as ScriptEngine_VerifyScriptWithCustomFlags
 *
 * Parameters:
 *   cgoBatch - The batch to add to
 *   extendedTxPtr - Pointer to extended transaction binary data
 *   extendedTxLen - Length of extended transaction data
 *   hUTXOsPtr - Pointer to array of UTXO heights
 *   hUTXOsLen - Number of elements in UTXO heights array
 *   blockHeight - Block height for verification
 *   consensus - Consensus flag
 *   cFlagsPtr - Pointer to custom flags array (can be NULL)
 *   cFlagsLen - Number of elements in custom flags array (0 if cFlagsPtr is NULL)
 */
void VerifyBatch_Add(
    VerifyBatchCGO cgoBatch,
    const char* extendedTxPtr, int extendedTxLen,
    const int32_t* hUTXOsPtr, int hUTXOsLen,
    int32_t blockHeight,
    bool consensus,
    const uint32_t* cFlagsPtr, int cFlagsLen
);

/**
 * VerifyBatch_Clear removes all elements from the batch
 * The batch can be reused after clearing
 */
void VerifyBatch_Clear(VerifyBatchCGO cgoBatch);

/**
 * VerifyBatch_Size returns the number of elements in the batch
 */
int VerifyBatch_Size(VerifyBatchCGO cgoBatch);

/**
 * VerifyBatch_Empty checks if the batch is empty
 * Returns true if the batch contains no elements
 */
bool VerifyBatch_Empty(VerifyBatchCGO cgoBatch);

/**
 * VerifyBatch_Reserve pre-allocates capacity for the specified number of elements
 * This is an optimization to avoid multiple reallocations when batch size is known
 */
void VerifyBatch_Reserve(VerifyBatchCGO cgoBatch, int capacity);

#ifdef __cplusplus
}
#endif

#endif /* __VERIFYBATCH_CGO_H__ */
