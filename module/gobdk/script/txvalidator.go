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
//
// This constructor has no error channel: a network name whose length cannot cross the
// C ABI is reported the same way a rejected network name already is, by returning nil.
func NewTxValidator(netName string) *TxValidator {
	netNameLen := len(netName)
	cNetNameLen, err := toABILen(netNameLen)
	if err != nil {
		return nil
	}

	netNameCstr := C.CString(netName)
	defer C.free(unsafe.Pointer(netNameCstr))

	// Create a new C++ TxValidator and bind it to the go struct
	goSE := &TxValidator{
		cSEPtr: C.TxValidator_CreateV2(netNameCstr, cNetNameLen),
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
	txPtr, lenTx, err := abiBuffer(extendedTX)
	if err != nil {
		return 0, err
	}

	utxoPtr, lenUtxo, err := abiInt32Buffer(utxoHeights)
	if err != nil {
		return 0, err
	}

	var errMsg *C.char
	sigOpCount := C.TxValidator_GetSigOpCount(se.cSEPtr, txPtr, lenTx, utxoPtr, lenUtxo, C.int32_t(blockHeight), C.bool(countP2SHSigOps), C.bool(consensus), &errMsg)
	runtime.KeepAlive(se)
	runtime.KeepAlive(extendedTX)
	runtime.KeepAlive(utxoHeights)

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
	txPtr, lenTx, err := abiBuffer(extendedTX)
	if err != nil {
		return err
	}

	utxoPtr, lenUtxo, err := abiInt32Buffer(utxoHeights)
	if err != nil {
		return err
	}

	result := C.TxValidator_VerifyScript(se.cSEPtr, txPtr, lenTx, utxoPtr, lenUtxo, C.int32_t(blockHeight), C.bool(consensus))
	runtime.KeepAlive(se)
	runtime.KeepAlive(extendedTX)
	runtime.KeepAlive(utxoHeights)
	return translateTxError(result)
}

// VerifyScriptWithCustomFlags calls VerifyScript with an additional custom flags array.
// This is usually used in tests or to experiment with flags other than the implicitly calculated ones.
func (se *TxValidator) VerifyScriptWithCustomFlags(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool, customFlags []uint32) error {
	txPtr, lenTx, err := abiBuffer(extendedTX)
	if err != nil {
		return err
	}

	utxoPtr, lenUtxo, err := abiInt32Buffer(utxoHeights)
	if err != nil {
		return err
	}

	flagsPtr, lenFlags, err := abiUint32Buffer(customFlags)
	if err != nil {
		return err
	}

	result := C.TxValidator_VerifyScriptWithCustomFlags(se.cSEPtr, txPtr, lenTx, utxoPtr, lenUtxo, C.int32_t(blockHeight), C.bool(consensus), flagsPtr, lenFlags)
	runtime.KeepAlive(se)
	runtime.KeepAlive(extendedTX)
	runtime.KeepAlive(utxoHeights)
	runtime.KeepAlive(customFlags)
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

// SetMaxSigOpsPolicy sets the pre-Genesis sigops policy limit in the C++ TxValidator.
func (se *TxValidator) SetMaxSigOpsPolicy(value uint64) {
	C.TxValidator_SetMaxSigOpsPolicy(se.cSEPtr, C.uint64_t(value))
	runtime.KeepAlive(se)
}

// SetMinConsolidationFactor sets the minimum consolidation factor.
// Negative values return an error. 0 stored literally (disables consolidation).
func (se *TxValidator) SetMinConsolidationFactor(value int64) error {
	errCStr := C.TxValidator_SetMinConsolidationFactor(se.cSEPtr, C.int64_t(value))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxConsolidationInputScriptSize sets the per-input scriptSig size cap for free
// consolidations. Negative values return an error. 0 resets to the bitcoin-sv default (150).
func (se *TxValidator) SetMaxConsolidationInputScriptSize(value int64) error {
	errCStr := C.TxValidator_SetMaxConsolidationInputScriptSize(se.cSEPtr, C.int64_t(value))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMinConfConsolidationInput sets the minimum confirmation count for consolidation inputs.
// Negative values return an error. 0 resets to the bitcoin-sv default (6).
func (se *TxValidator) SetMinConfConsolidationInput(value int64) error {
	errCStr := C.TxValidator_SetMinConfConsolidationInput(se.cSEPtr, C.int64_t(value))
	runtime.KeepAlive(se)
	if errCStr == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetAcceptNonStdConsolidationInput toggles acceptance of non-standard inputs in consolidations.
func (se *TxValidator) SetAcceptNonStdConsolidationInput(value bool) {
	C.TxValidator_SetAcceptNonStdConsolidationInput(se.cSEPtr, C.bool(value))
	runtime.KeepAlive(se)
}

// SetMinMiningTxFee sets the static fee floor in integer satoshis/kB.
// 0 means "no fee policy" (every tx passes the floor). Negative returns an error.
// Callers converting from float BSV/kB should use math.Round(rate * 1e8) (not truncation)
// because IEEE-754 representations of decimal values can drop one satoshi.
func (se *TxValidator) SetMinMiningTxFee(satoshisPerKB int64) error {
	errCStr := C.TxValidator_SetMinMiningTxFee(se.cSEPtr, C.int64_t(satoshisPerKB))
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

// ValidateBatch processes a batch of transaction validations.
// Returns a slice of errors, one per batch entry, in the same order.
// Each element may be nil (success), a ScriptError, a DoSError, an ABIError, or a
// generic exception error.
//
// NOT CONCURRENT-SAFE: Must be called from the same goroutine that built the batch
// via ValidateBatch.Add (or externally serialized). See ValidateBatch for details.
func (se *TxValidator) ValidateBatch(batch *ValidateBatch) []error {
	if batch == nil || batch.cBatchPtr == nil {
		return nil
	}

	var resultSize C.uint64_t
	resultsPtr := C.TxValidator_ValidateBatch(se.cSEPtr, batch.cBatchPtr, &resultSize)
	runtime.KeepAlive(se)
	runtime.KeepAlive(batch)

	if resultsPtr == nil || resultSize == 0 {
		return nil
	}

	defer C.free(unsafe.Pointer(resultsPtr))

	// The count is bounded by the batch size, but it arrives in the ABI length type:
	// convert it rather than assume it fits.
	size, err := abiToGoLen(resultSize)
	if err != nil {
		return []error{err}
	}

	results := make([]error, size)

	cResults := unsafe.Slice((*C.TxError)(resultsPtr), size)
	for i := 0; i < size; i++ {
		results[i] = translateTxError(cResults[i])
	}

	return results
}

// ValidateTransaction runs all tx-level checks then script verification.
// consensus=false → peer context (policy + consensus checks)
// consensus=true  → block context (consensus checks only)
// Returns nil on success, DoSError or ScriptError on failure, and ABIError when an
// argument could not be expressed across the C boundary — a local fault of this
// binding, not a verdict on the transaction.
func (se *TxValidator) ValidateTransaction(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) error {
	txPtr, lenTx, err := abiBuffer(extendedTX)
	if err != nil {
		return err
	}

	utxoPtr, lenUtxo, err := abiInt32Buffer(utxoHeights)
	if err != nil {
		return err
	}

	result := C.TxValidator_ValidateTransaction(se.cSEPtr, txPtr, lenTx, utxoPtr, lenUtxo, C.int32_t(blockHeight), C.bool(consensus))
	runtime.KeepAlive(se)
	runtime.KeepAlive(extendedTX)
	runtime.KeepAlive(utxoHeights)
	return translateTxError(result)
}

// ABIEchoLength returns the length it was given, after a round trip through the C ABI.
// It exists so a test can prove that a buffer length crosses the boundary unchanged,
// for values a C int could never have carried.
func ABIEchoLength(n uint64) uint64 {
	return uint64(C.TxValidator_ABI_EchoLength(C.uint64_t(n)))
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
	case C.TX_ERR_DOMAIN_ABI:
		return NewABIError(ABIErrorCode(r.code))
	default:
		return fmt.Errorf("unknown TxError domain=%d code=%d", r.domain, r.code)
	}
}
