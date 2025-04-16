package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <stdlib.h>
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"errors"
	"runtime"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// ScriptEngine map to the C++ class ScriptEngine
type ScriptEngine struct {
	cSEPtr C.ScriptEngineCGO
}

// NewScriptEngine creates a new ScriptEngine, and set the Finalizer to call C++ destructor
func NewScriptEngine(netName string) *ScriptEngine {
	netNameLen := len(netName)
	netNameCstr := C.CString(netName)
	defer C.free(unsafe.Pointer(netNameCstr))

	// Create a new C++ ScriptEngine and bind it to the go struct
	goSE := &ScriptEngine{
		cSEPtr: C.ScriptEngine_Create(netNameCstr, C.int(netNameLen)),
	}

	// If C is not able to create the ScriptEngine, then return nil
	if goSE.cSEPtr == nil {
		return nil
	}

	// Set finalizer to delete C++ ScriptEngine when GC collects this struct
	runtime.SetFinalizer(goSE, func(obj *ScriptEngine) {
		if obj.cSEPtr != nil {
			C.ScriptEngine_Destroy(obj.cSEPtr)
			obj.cSEPtr = nil
		}
	})

	return goSE
}

// CalculateFlags calculates the flags to be used to verify the script
func (se *ScriptEngine) CalculateFlags(utxoHeight int32, blockHeight int32, consensus bool) uint32 {
	return uint32(C.ScriptEngine_CalculateFlags(se.cSEPtr, C.int32_t(utxoHeight), C.int32_t(blockHeight), C.bool(consensus)))
}

// VerifyScript verifies the script by providing
//   - The extended transaction
//   - The array of the utxo heights ( required to calculate the flags )
//   - The current block height
//   - The consensus parameter
func (se *ScriptEngine) VerifyScript(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) ScriptError {

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

	errCode := int(C.ScriptEngine_VerifyScript(se.cSEPtr, txPtr, C.int(lenTx), utxoPtr, C.int(lenUtxo), C.int32_t(blockHeight), C.bool(consensus)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}

	return NewScriptError(ScriptErrorCode(errCode))

}

// SetMaxOpsPerScriptPolicy set the MaxOpsPerScriptPolicy in the C++ ScriptEngine
func (se *ScriptEngine) SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn int64) error {
	errCStr := C.ScriptEngine_SetMaxOpsPerScriptPolicy(se.cSEPtr, C.int64_t(maxOpsPerScriptPolicyIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxScriptNumLengthPolicy set the MaxScriptNumLengthPolicy in the C++ ScriptEngine
func (se *ScriptEngine) SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn int64) error {
	errCStr := C.ScriptEngine_SetMaxScriptNumLengthPolicy(se.cSEPtr, C.int64_t(maxScriptNumLengthIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))
	return errors.New(C.GoString(errCStr))
}

// SetMaxScriptSizePolicy set the MaxScriptSizePolicy in the C++ ScriptEngine
func (se *ScriptEngine) SetMaxScriptSizePolicy(maxScriptSizePolicyIn int64) error {
	errCStr := C.ScriptEngine_SetMaxScriptSizePolicy(se.cSEPtr, C.int64_t(maxScriptSizePolicyIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))

	return errors.New(C.GoString(errCStr))
}

// SetMaxPubKeysPerMultiSigPolicy set the MaxPubKeysPerMultiSigPolicy in the C++ ScriptEngine
func (se *ScriptEngine) SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn int64) error {
	errCStr := C.ScriptEngine_SetMaxPubKeysPerMultiSigPolicy(se.cSEPtr, C.int64_t(maxPubKeysPerMultiSigIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))

	return errors.New(C.GoString(errCStr))
}

// SetMaxStackMemoryUsage set the MaxStackMemoryUsage in the C++ ScriptEngine
func (se *ScriptEngine) SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn int64, maxStackMemoryUsagePolicyIn int64) error {
	errCStr := C.ScriptEngine_SetMaxStackMemoryUsage(se.cSEPtr, C.int64_t(maxStackMemoryUsageConsensusIn), C.int64_t(maxStackMemoryUsagePolicyIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))

	return errors.New(C.GoString(errCStr))
}

// SetGenesisActivationHeight set the GenesisActivationHeight in the C++ ScriptEngine
func (se *ScriptEngine) SetGenesisActivationHeight(genesisActivationHeightIn int32) error {
	errCStr := C.ScriptEngine_SetGenesisActivationHeight(se.cSEPtr, C.int32_t(genesisActivationHeightIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))

	return errors.New(C.GoString(errCStr))
}

// SetChronicleActivationHeight set the ChronicleActivationHeight in the C++ ScriptEngine
func (se *ScriptEngine) SetChronicleActivationHeight(chronicleActivationHeightIn int32) error {
	errCStr := C.ScriptEngine_SetChronicleActivationHeight(se.cSEPtr, C.int32_t(chronicleActivationHeightIn))

	if errCStr == nil {
		return nil
	}

	defer C.free(unsafe.Pointer(errCStr))

	return errors.New(C.GoString(errCStr))
}

// GetMaxOpsPerScript get the MaxOpsPerScript being set
func (se *ScriptEngine) GetMaxOpsPerScript(isGenesisEnabled, isConsensus bool) uint64 {
	return uint64(C.ScriptEngine_GetMaxOpsPerScript(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus)))
}

// GetMaxScriptNumLength get the MaxScriptNumLength being set
func (se *ScriptEngine) GetMaxScriptNumLength(isGenesisEnabled, isChronicleEnabled, isConsensus bool) uint64 {
	return uint64(C.ScriptEngine_GetMaxScriptNumLength(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isChronicleEnabled), C.bool(isConsensus)))
}

// GetMaxScriptSize get the MaxScriptSize being set
func (se *ScriptEngine) GetMaxScriptSize(isGenesisEnabled, isConsensus bool) uint64 {
	return uint64(C.ScriptEngine_GetMaxScriptSize(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus)))
}

// GetMaxPubKeysPerMultiSig get the MaxPubKeysPerMultiSig being set
func (se *ScriptEngine) GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus bool) uint64 {
	return uint64(C.ScriptEngine_GetMaxPubKeysPerMultiSig(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus)))
}

// GetMaxStackMemoryUsage get the MaxStackMemoryUsage being set
func (se *ScriptEngine) GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus bool) uint64 {
	return uint64(C.ScriptEngine_GetMaxStackMemoryUsage(se.cSEPtr, C.bool(isGenesisEnabled), C.bool(isConsensus)))
}

// GetGenesisActivationHeight get the genesis height being set
func (se *ScriptEngine) GetGenesisActivationHeight() int32 {
	return int32(C.ScriptEngine_GetGenesisActivationHeight(se.cSEPtr))
}

// GetChronicleActivationHeight get the chronicle height being set
func (se *ScriptEngine) GetChronicleActivationHeight() int32 {
	return int32(C.ScriptEngine_GetChronicleActivationHeight(se.cSEPtr))
}
