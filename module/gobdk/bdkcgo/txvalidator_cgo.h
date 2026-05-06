#ifndef __TXVALIDATOR_CGO_H__
#define __TXVALIDATOR_CGO_H__

#include <stdint.h>
#include <stdbool.h>
#include <bdkcgo/txerror_cgo.h>
#include <bdkcgo/doserror_cgo.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * TxValidator_CPP_SCRIPT_ERR_ERROR_COUNT return the SCRIPT_ERR_ERROR_COUNT as defined in the C++ code
 * This is for the testing purpose, in case the list of error code has changed, then the tester in
 * other binding language will be notified by a test
 */
int TxValidator_CPP_SCRIPT_ERR_ERROR_COUNT();

/**
 * TxValidatorCGO implement the cgo handler for TxValidator class
 */
typedef void* TxValidatorCGO;

/**
 * Forward declaration for VerifyBatchCGO (defined in verifybatch_cgo.h)
 */
typedef void* VerifyBatchCGO;

/**
 * Handle constructor and destructor
 */
TxValidatorCGO TxValidator_Create(const char* networkNamePtr, int networkNameLen);
void TxValidator_Destroy(TxValidatorCGO cgoEngine);

/**
 * Settings for script engine. It returns nullptr if the setting is ok
 * 
 * Otherwise it returns an error string as a C-String null terminated
 * Client code must delete the returned C-String
 */
const char* TxValidator_SetMaxOpsPerScriptPolicy(TxValidatorCGO cgoEngine, int64_t maxOpsPerScriptPolicyIn);
const char* TxValidator_SetMaxScriptNumLengthPolicy(TxValidatorCGO cgoEngine, int64_t maxScriptNumLengthIn);
const char* TxValidator_SetMaxScriptSizePolicy(TxValidatorCGO cgoEngine, int64_t maxScriptSizePolicyIn);
const char* TxValidator_SetMaxPubKeysPerMultiSigPolicy(TxValidatorCGO cgoEngine, int64_t maxPubKeysPerMultiSigIn);
const char* TxValidator_SetMaxStackMemoryUsage(TxValidatorCGO cgoEngine, int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn);
const char* TxValidator_SetGenesisActivationHeight(TxValidatorCGO cgoEngine, int32_t genesisActivationHeightIn);
const char* TxValidator_SetChronicleActivationHeight(TxValidatorCGO cgoEngine, int32_t chronicleActivationHeightIn);
const char* TxValidator_SetGenesisGracefulPeriod(TxValidatorCGO cgoEngine, int64_t genesisGracefulPeriodIn);
const char* TxValidator_SetChronicleGracefulPeriod(TxValidatorCGO cgoEngine, int64_t chronicleGracefulPeriodIn);
const char* TxValidator_SetMaxTxSizePolicy(TxValidatorCGO cgoEngine, int64_t value);
const char* TxValidator_SetMaxSigOpsPostGenesisPolicy(TxValidatorCGO cgoEngine, int64_t value);
void TxValidator_SetDataCarrierSize(TxValidatorCGO cgoEngine, uint64_t dataCarrierSize);
void TxValidator_SetDataCarrier(TxValidatorCGO cgoEngine, bool dataCarrier);
void TxValidator_SetAcceptNonStandardOutput(TxValidatorCGO cgoEngine, bool accept);
void TxValidator_SetRequireStandard(TxValidatorCGO cgoEngine, bool require);
void TxValidator_SetPermitBareMultisig(TxValidatorCGO cgoEngine, bool permit);
void TxValidator_ResetDefault(TxValidatorCGO cgoEngine);

// Forward getter to GlobalConfig call
uint64_t TxValidator_GetMaxOpsPerScript(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool consensus);
uint64_t TxValidator_GetMaxScriptNumLength(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus);
uint64_t TxValidator_GetMaxScriptSize(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isConsensus);
uint64_t TxValidator_GetMaxPubKeysPerMultiSig(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool consensus);
uint64_t TxValidator_GetMaxStackMemoryUsage(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool consensus);
uint64_t TxValidator_GetMaxTxSize(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus);
uint64_t TxValidator_GetDataCarrierSize(TxValidatorCGO cgoEngine);
bool TxValidator_GetDataCarrier(TxValidatorCGO cgoEngine);
bool TxValidator_GetAcceptNonStandardOutput(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled);
bool TxValidator_GetRequireStandard(TxValidatorCGO cgoEngine);
bool TxValidator_GetPermitBareMultisig(TxValidatorCGO cgoEngine);
int32_t TxValidator_GetGenesisActivationHeight(TxValidatorCGO cgoEngine);
int32_t TxValidator_GetChronicleActivationHeight(TxValidatorCGO cgoEngine);
uint64_t TxValidator_GetGenesisGracefulPeriod(TxValidatorCGO cgoEngine);
uint64_t TxValidator_GetChronicleGracefulPeriod(TxValidatorCGO cgoEngine);

/*
 * TxValidator_GetSigOpCount calculate the number of sigops in a transaction
 *
 * Inputs C-Style
 *   - Extended Transaction binary
 *   - Array of utxo heights
 *   - Block Height
 *   - countP2SHSigOps: pass (blockFlags & SCRIPT_VERIFY_P2SH) != 0 for block-level
 *     aggregate counting; pass true for policy/mempool use.
 *
 * Caller must free errStr
 */
uint64_t TxValidator_GetSigOpCount(TxValidatorCGO cgoEngine,
	const char* extendedTxPtr, int extendedTxLen,
	const int32_t* hUTXOsPtr, int hUTXOsLen,
	int32_t blockHeight,
	bool countP2SHSigOps,
	bool consensus,
	char** errStr
);

/*
 * CalculateFlags helper to calculate the flags manually
 * Use for test and debug purpose
 */
uint32_t TxValidator_CalculateFlags(TxValidatorCGO cgoEngine, int32_t utxoHeight, int32_t blockHeight, bool consensus);

/*
 * VerifyScript take inputs in C-style and forward the input to the C++ call.
 *
 * Inputs C-Style
 *   - Extended Transaction binary
 *   - Array of utxo heights ( required to calculate flags )
 *   - Block Height
 *   - Consensus toggle
 */
TxError TxValidator_VerifyScript(TxValidatorCGO cgoEngine,
	const char* extendedTxPtr, int extendedTxLen,
	const int32_t* hUTXOsPtr, int hUTXOsLen,
	int32_t blockHeight, bool consensus
);

/*
 * VerifyScriptWithCustomFlags takes the same inputs as VerifyScript, with an
 * additional custom flags array.
 */
TxError TxValidator_VerifyScriptWithCustomFlags(TxValidatorCGO cgoEngine,
	const char* extendedTxPtr, int extendedTxLen,
	const int32_t* hUTXOsPtr, int hUTXOsLen,
	int32_t blockHeight, bool consensus,
	const uint32_t* cFlagsPtr, int cFlagsLen
);

/*
 * VerifyScriptBatch processes a batch of script verifications.
 *
 * Returns a pointer to a malloc'd array of TxError structs, one per batch entry.
 * The caller must free the returned array using free().
 * resultSize is set to the number of elements.
 */
TxError* TxValidator_VerifyScriptBatch(TxValidatorCGO cgoEngine, VerifyBatchCGO cgoBatch, int* resultSize);

/*
 * TxValidator_CheckTransaction runs all tx-level checks then script verification.
 * consensus=false → peer context (policy + consensus checks)
 * consensus=true  → block context (consensus checks only)
 *
 * Returns a TxError struct: { domain, code }.
 * domain=TX_ERR_DOMAIN_OK on success.
 */
TxError TxValidator_CheckTransaction(TxValidatorCGO cgoEngine,
    const char* extendedTxPtr, int extendedTxLen,
    const int32_t* hUTXOsPtr, int hUTXOsLen,
    int32_t blockHeight, bool consensus);

#ifdef __cplusplus
}
#endif

#endif /* __TXVALIDATOR_CGO_H__ */