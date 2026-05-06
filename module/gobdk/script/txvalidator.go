package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <stdlib.h>
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// TxValidator map to the C++ class TxValidator
type TxValidator struct {
	cSEPtr C.TxValidatorCGO
}

// NewTxValidator creates a new TxValidator, and set the Finalizer to call C++ destructor
func NewTxValidator(netName string) *TxValidator {
	netNameLen := len(netName)
	netNameCstr := C.CString(netName)
	defer C.free(unsafe.Pointer(netNameCstr))

	// Create a new C++ TxValidator and bind it to the go struct
	goSE := &TxValidator{
		cSEPtr: C.TxValidator_Create(netNameCstr, C.int(netNameLen)),
	}

	// If C is not able to create the TxValidator, then return nil
	if goSE.cSEPtr == nil {
		return nil
	}

	// Set finalizer to delete C++ TxValidator when GC collects this struct
	runtime.SetFinalizer(goSE, func(obj *TxValidator) {
		if obj.cSEPtr != nil {
			C.TxValidator_Destroy(obj.cSEPtr)
			obj.cSEPtr = nil
		}
	})

	return goSE
}

// GetSigOpCount returns the number of sigops in an extended transaction.
// countP2SHSigOps should be (blockFlags & SCRIPT_VERIFY_P2SH) != 0 for block-level
// aggregate counting, or true for policy/mempool use.
// consensus=false uses blockHeight+1 era (policy); consensus=true uses blockHeight era (block validation).
func (se *TxValidator) GetSigOpCount(extendedTX []byte, utxoHeights []int32, blockHeight int32, countP2SHSigOps bool, consensus bool) (uint64, error) {
	lenTx := len(extendedTX)
	var txPtr *C.char

	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&extendedTX[0]))
	}

	lenUtxo := len(utxoHeights)

	var utxoPtr *C.int32_t
	if lenUtxo > 0 {
		utxoPtr = (*C.int32_t)(unsafe.Pointer(&utxoHeights[0]))
	}

	var errMsg *C.char
	sigOpCount := C.TxValidator_GetSigOpCount(se.cSEPtr, txPtr, C.int(lenTx), utxoPtr, C.int(lenUtxo), C.int32_t(blockHeight), C.bool(countP2SHSigOps), C.bool(consensus), &errMsg)
	runtime.KeepAlive(se)

	if errMsg != nil {
		defer C.free(unsafe.Pointer(errMsg))
		return uint64(sigOpCount), errors.New(C.GoString(errMsg))
	}

	return uint64(sigOpCount), nil
}

// CalculateFlags calculates the flags to be used to verify the script
func (se *TxValidator) CalculateFlags(utxoHeight int32, blockHeight int32, consensus bool) uint32 {
	result := C.TxValidator_CalculateFlags(se.cSEPtr, C.int32_t(utxoHeight), C.int32_t(blockHeight), C.bool(consensus))
	runtime.KeepAlive(se)
	return uint32(result)
}

// VerifyScript verifies the script by providing
//   - The extended transaction
//   - The array of the utxo heights ( required to calculate the flags )
//   - The current block height
//   - The consensus parameter
func (se *TxValidator) VerifyScript(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) error {
	lenTx := len(extendedTX)
	var txPtr *C.char
	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&extendedTX[0]))
	}

	lenUtxo := len(utxoHeights)
	var utxoPtr *C.int32_t
	if lenUtxo > 0 {
		utxoPtr = (*C.int32_t)(unsafe.Pointer(&utxoHeights[0]))
	}

	result := C.TxValidator_VerifyScript(se.cSEPtr, txPtr, C.int(lenTx), utxoPtr, C.int(lenUtxo), C.int32_t(blockHeight), C.bool(consensus))
	runtime.KeepAlive(se)
	return translateTxError(result)
}

// VerifyScriptWithCustomFlags calls VerifyScript with an additional custom flags array.
// This is usually used in tests or to experiment with flags other than the implicitly calculated ones.
func (se *TxValidator) VerifyScriptWithCustomFlags(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool, customFlags []uint32) error {
	lenTx := len(extendedTX)
	var txPtr *C.char
	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&extendedTX[0]))
	}

	lenUtxo := len(utxoHeights)
	var utxoPtr *C.int32_t
	if lenUtxo > 0 {
		utxoPtr = (*C.int32_t)(unsafe.Pointer(&utxoHeights[0]))
	}

	lenFlags := len(customFlags)
	var flagsPtr *C.uint32_t
	if lenFlags > 0 {
		flagsPtr = (*C.uint32_t)(unsafe.Pointer(&customFlags[0]))
	}

	result := C.TxValidator_VerifyScriptWithCustomFlags(se.cSEPtr, txPtr, C.int(lenTx), utxoPtr, C.int(lenUtxo), C.int32_t(blockHeight), C.bool(consensus), flagsPtr, C.int(lenFlags))
	runtime.KeepAlive(se)
	return translateTxError(result)
}

// SetMaxOpsPerScriptPolicy set the MaxOpsPerScriptPolicy in the C++ TxValidator
func (se *TxValidator) SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn int64) error {
	errCStr := C.TxValidator_SetMaxOpsPerScriptPolicy(se.cSEPtr, C.int64_t(maxOpsPerScriptPolicyIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxScriptNumLengthPolicy set the MaxScriptNumLengthPolicy in the C++ TxValidator
func (se *TxValidator) SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn int64) error {
	errCStr := C.TxValidator_SetMaxScriptNumLengthPolicy(se.cSEPtr, C.int64_t(maxScriptNumLengthIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxScriptSizePolicy set the MaxScriptSizePolicy in the C++ TxValidator
func (se *TxValidator) SetMaxScriptSizePolicy(maxScriptSizePolicyIn int64) error {
	errCStr := C.TxValidator_SetMaxScriptSizePolicy(se.cSEPtr, C.int64_t(maxScriptSizePolicyIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxPubKeysPerMultiSigPolicy set the MaxPubKeysPerMultiSigPolicy in the C++ TxValidator
func (se *TxValidator) SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn int64) error {
	errCStr := C.TxValidator_SetMaxPubKeysPerMultiSigPolicy(se.cSEPtr, C.int64_t(maxPubKeysPerMultiSigIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxStackMemoryUsage set the MaxStackMemoryUsage in the C++ TxValidator
func (se *TxValidator) SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn int64, maxStackMemoryUsagePolicyIn int64) error {
	errCStr := C.TxValidator_SetMaxStackMemoryUsage(se.cSEPtr, C.int64_t(maxStackMemoryUsageConsensusIn), C.int64_t(maxStackMemoryUsagePolicyIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetGenesisActivationHeight set the GenesisActivationHeight in the C++ TxValidator
func (se *TxValidator) SetGenesisActivationHeight(genesisActivationHeightIn int32) error {
	errCStr := C.TxValidator_SetGenesisActivationHeight(se.cSEPtr, C.int32_t(genesisActivationHeightIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetChronicleActivationHeight set the ChronicleActivationHeight in the C++ TxValidator
func (se *TxValidator) SetChronicleActivationHeight(chronicleActivationHeightIn int32) error {
	errCStr := C.TxValidator_SetChronicleActivationHeight(se.cSEPtr, C.int32_t(chronicleActivationHeightIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// GetMaxOpsPerScript get the MaxOpsPerScript being set
func (se *TxValidator) GetMaxOpsPerScript(isGenesisEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxOpsPerScript(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetMaxScriptNumLength get the MaxScriptNumLength being set
func (se *TxValidator) GetMaxScriptNumLength(isGenesisEnabled, isChronicleEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxScriptNumLength(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isChronicleEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetMaxScriptSize get the MaxScriptSize being set
func (se *TxValidator) GetMaxScriptSize(isGenesisEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxScriptSize(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetMaxPubKeysPerMultiSig get the MaxPubKeysPerMultiSig being set
func (se *TxValidator) GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxPubKeysPerMultiSig(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetMaxStackMemoryUsage get the MaxStackMemoryUsage being set
func (se *TxValidator) GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxStackMemoryUsage(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetGenesisActivationHeight get the genesis height being set
func (se *TxValidator) GetGenesisActivationHeight() int32 {
	result := C.TxValidator_GetGenesisActivationHeight(se.cSEPtr)
	runtime.KeepAlive(se)
	return int32(result)
}

// GetChronicleActivationHeight get the chronicle height being set
func (se *TxValidator) GetChronicleActivationHeight() int32 {
	result := C.TxValidator_GetChronicleActivationHeight(se.cSEPtr)
	runtime.KeepAlive(se)
	return int32(result)
}

// GetGenesisGracefulPeriod get the genesis graceful period being set
func (se *TxValidator) GetGenesisGracefulPeriod() uint64 {
	result := C.TxValidator_GetGenesisGracefulPeriod(se.cSEPtr)
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetChronicleGracefulPeriod get the chronicle graceful period being set
func (se *TxValidator) GetChronicleGracefulPeriod() uint64 {
	result := C.TxValidator_GetChronicleGracefulPeriod(se.cSEPtr)
	runtime.KeepAlive(se)
	return uint64(result)
}

// SetGenesisGracefulPeriod set the GenesisGracefulPeriod in the C++ TxValidator
func (se *TxValidator) SetGenesisGracefulPeriod(genesisGracefulPeriodIn int64) error {
	errCStr := C.TxValidator_SetGenesisGracefulPeriod(se.cSEPtr, C.int64_t(genesisGracefulPeriodIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetChronicleGracefulPeriod set the ChronicleGracefulPeriod in the C++ TxValidator
func (se *TxValidator) SetChronicleGracefulPeriod(chronicleGracefulPeriodIn int64) error {
	errCStr := C.TxValidator_SetChronicleGracefulPeriod(se.cSEPtr, C.int64_t(chronicleGracefulPeriodIn))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxTxSizePolicy set the MaxTxSizePolicy in the C++ TxValidator
func (se *TxValidator) SetMaxTxSizePolicy(value int64) error {
	errCStr := C.TxValidator_SetMaxTxSizePolicy(se.cSEPtr, C.int64_t(value))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxSigOpsPostGenesisPolicy sets the post-Genesis sigops policy limit.
// 0 resets to the default (UINT32_MAX / unlimited). Negative or > UINT32_MAX returns an error.
func (se *TxValidator) SetMaxSigOpsPostGenesisPolicy(value int64) error {
	errCStr := C.TxValidator_SetMaxSigOpsPostGenesisPolicy(se.cSEPtr, C.int64_t(value))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetDataCarrierSize set the DataCarrierSize in the C++ TxValidator
func (se *TxValidator) SetDataCarrierSize(dataCarrierSize uint64) {
	C.TxValidator_SetDataCarrierSize(se.cSEPtr, C.uint64_t(dataCarrierSize))
	runtime.KeepAlive(se)
}

// SetDataCarrier set the DataCarrier flag in the C++ TxValidator
func (se *TxValidator) SetDataCarrier(dataCarrier bool) {
	C.TxValidator_SetDataCarrier(se.cSEPtr, C.bool(dataCarrier))
	runtime.KeepAlive(se)
}

// SetAcceptNonStandardOutput set the AcceptNonStandardOutput flag in the C++ TxValidator
func (se *TxValidator) SetAcceptNonStandardOutput(accept bool) {
	C.TxValidator_SetAcceptNonStandardOutput(se.cSEPtr, C.bool(accept))
	runtime.KeepAlive(se)
}

// SetRequireStandard set the RequireStandard flag in the C++ TxValidator
func (se *TxValidator) SetRequireStandard(require bool) {
	C.TxValidator_SetRequireStandard(se.cSEPtr, C.bool(require))
	runtime.KeepAlive(se)
}

// SetPermitBareMultisig set the PermitBareMultisig flag in the C++ TxValidator
func (se *TxValidator) SetPermitBareMultisig(permit bool) {
	C.TxValidator_SetPermitBareMultisig(se.cSEPtr, C.bool(permit))
	runtime.KeepAlive(se)
}

// ResetDefault resets all policy settings to their default values
func (se *TxValidator) ResetDefault() {
	C.TxValidator_ResetDefault(se.cSEPtr)
	runtime.KeepAlive(se)
}

// GetMaxTxSize get the MaxTxSize for the given protocol era
func (se *TxValidator) GetMaxTxSize(isGenesisEnabled, isChronicleEnabled, isConsensus bool) uint64 {
	result := C.TxValidator_GetMaxTxSize(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isChronicleEnabled), C.bool(isConsensus))
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetDataCarrierSize get the DataCarrierSize being set
func (se *TxValidator) GetDataCarrierSize() uint64 {
	result := C.TxValidator_GetDataCarrierSize(se.cSEPtr)
	runtime.KeepAlive(se)
	return uint64(result)
}

// GetDataCarrier get the DataCarrier flag being set
func (se *TxValidator) GetDataCarrier() bool {
	result := C.TxValidator_GetDataCarrier(se.cSEPtr)
	runtime.KeepAlive(se)
	return bool(result)
}

// GetAcceptNonStandardOutput get the AcceptNonStandardOutput flag for the given protocol era
func (se *TxValidator) GetAcceptNonStandardOutput(isGenesisEnabled, isChronicleEnabled bool) bool {
	result := C.TxValidator_GetAcceptNonStandardOutput(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isChronicleEnabled))
	runtime.KeepAlive(se)
	return bool(result)
}

// GetRequireStandard get the RequireStandard flag being set
func (se *TxValidator) GetRequireStandard() bool {
	result := C.TxValidator_GetRequireStandard(se.cSEPtr)
	runtime.KeepAlive(se)
	return bool(result)
}

// GetPermitBareMultisig get the PermitBareMultisig flag being set
func (se *TxValidator) GetPermitBareMultisig() bool {
	result := C.TxValidator_GetPermitBareMultisig(se.cSEPtr)
	runtime.KeepAlive(se)
	return bool(result)
}

// VerifyScriptBatch processes a batch of script verifications.
// Returns a slice of errors, one per batch entry, in the same order.
// Each element may be nil (success), a ScriptError, a DoSError, or a generic exception error.
func (se *TxValidator) VerifyScriptBatch(batch *VerifyBatch) []error {
	if batch == nil || batch.cBatchPtr == nil {
		return nil
	}

	var resultSize C.int
	resultsPtr := C.TxValidator_VerifyScriptBatch(se.cSEPtr, batch.cBatchPtr, &resultSize)
	runtime.KeepAlive(se)
	runtime.KeepAlive(batch)

	if resultsPtr == nil || resultSize == 0 {
		return nil
	}

	defer C.free(unsafe.Pointer(resultsPtr))

	size := int(resultSize)
	results := make([]error, size)

	cResults := unsafe.Slice((*C.TxError)(resultsPtr), size)
	for i := 0; i < size; i++ {
		results[i] = translateTxError(cResults[i])
	}

	return results
}

// CheckTransaction runs all tx-level checks then script verification.
// consensus=false → peer context (policy + consensus checks)
// consensus=true  → block context (consensus checks only)
// Returns nil on success, DoSError or ScriptError on failure.
func (se *TxValidator) CheckTransaction(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) error {
	lenTx := len(extendedTX)
	var txPtr *C.char
	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&extendedTX[0]))
	}

	lenUtxo := len(utxoHeights)
	var utxoPtr *C.int32_t
	if lenUtxo > 0 {
		utxoPtr = (*C.int32_t)(unsafe.Pointer(&utxoHeights[0]))
	}

	result := C.TxValidator_CheckTransaction(se.cSEPtr, txPtr, C.int(lenTx), utxoPtr, C.int(lenUtxo), C.int32_t(blockHeight), C.bool(consensus))
	runtime.KeepAlive(se)
	return translateTxError(result)
}

// translateTxError converts a C TxError into a Go error. Returns nil on success.
func translateTxError(r C.TxError) error {
	switch r.domain {
	case C.TX_ERR_DOMAIN_OK:
		return nil
	case C.TX_ERR_DOMAIN_SCRIPT:
		return NewScriptError(ScriptErrorCode(r.code))
	case C.TX_ERR_DOMAIN_DOS:
		return NewDoSError(DoSErrorCode(r.code))
	case C.TX_ERR_DOMAIN_EXCEPTION:
		return NewScriptError(SCRIPT_ERR_CGO_EXCEPTION)
	default:
		return fmt.Errorf("unknown TxError domain=%d code=%d", r.domain, r.code)
	}
}
