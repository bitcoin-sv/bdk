//go:build !cgo || purego

package script

import (
	"errors"
	"runtime"
	"unsafe"

	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

var (
	pScriptEngineCreate                      func(namePtr uintptr, nameLen int32) uintptr
	pScriptEngineDestroy                     func(engine uintptr)
	pScriptEngineSetMaxOpsPerScriptPolicy    func(engine uintptr, val int64) uintptr
	pScriptEngineSetMaxScriptNumLengthPolicy func(engine uintptr, val int64) uintptr
	pScriptEngineSetMaxScriptSizePolicy      func(engine uintptr, val int64) uintptr
	pScriptEngineSetMaxPubKeysPerMultiSig    func(engine uintptr, val int64) uintptr
	pScriptEngineSetMaxStackMemoryUsage      func(engine uintptr, consensus int64, policy int64) uintptr
	pScriptEngineSetGenesisActivationHeight  func(engine uintptr, val int32) uintptr
	pScriptEngineSetChronicleActivationHeight func(engine uintptr, val int32) uintptr
	pScriptEngineSetGenesisGracefulPeriod    func(engine uintptr, val int64) uintptr
	pScriptEngineSetChronicleGracefulPeriod  func(engine uintptr, val int64) uintptr
	pScriptEngineSetMaxTxSizePolicy          func(engine uintptr, val int64) uintptr
	pScriptEngineSetDataCarrierSize          func(engine uintptr, val uint64)
	pScriptEngineSetDataCarrier              func(engine uintptr, val bool)
	pScriptEngineSetAcceptNonStandardOutput  func(engine uintptr, val bool)
	pScriptEngineSetRequireStandard          func(engine uintptr, val bool)
	pScriptEngineSetPermitBareMultisig       func(engine uintptr, val bool)
	pScriptEngineResetDefault                func(engine uintptr)

	pScriptEngineGetMaxOpsPerScript      func(engine uintptr, isGenesis bool, consensus bool) uint64
	pScriptEngineGetMaxScriptNumLength   func(engine uintptr, isGenesis bool, isChronicle bool, isConsensus bool) uint64
	pScriptEngineGetMaxScriptSize        func(engine uintptr, isGenesis bool, isConsensus bool) uint64
	pScriptEngineGetMaxPubKeysPerMultiSig func(engine uintptr, isGenesis bool, consensus bool) uint64
	pScriptEngineGetMaxStackMemoryUsage  func(engine uintptr, isGenesis bool, consensus bool) uint64
	pScriptEngineGetMaxTxSize            func(engine uintptr, isGenesis bool, isChronicle bool, isConsensus bool) uint64
	pScriptEngineGetDataCarrierSize      func(engine uintptr) uint64
	pScriptEngineGetDataCarrier          func(engine uintptr) bool
	pScriptEngineGetAcceptNonStdOutput   func(engine uintptr, isGenesis bool, isChronicle bool) bool
	pScriptEngineGetRequireStandard      func(engine uintptr) bool
	pScriptEngineGetPermitBareMultisig   func(engine uintptr) bool
	pScriptEngineGetGenesisActivationHeight  func(engine uintptr) int32
	pScriptEngineGetChronicleActivationHeight func(engine uintptr) int32
	pScriptEngineGetGenesisGracefulPeriod    func(engine uintptr) uint64
	pScriptEngineGetChronicleGracefulPeriod  func(engine uintptr) uint64

	pScriptEngineVerifyScript              func(engine uintptr, txPtr uintptr, txLen int32, utxoPtr uintptr, utxoLen int32, blockHeight int32, consensus bool) int32
	pScriptEngineVerifyScriptWithCustomFlags func(engine uintptr, txPtr uintptr, txLen int32, utxoPtr uintptr, utxoLen int32, blockHeight int32, consensus bool, flagsPtr uintptr, flagsLen int32) int32
	pScriptEngineVerifyScriptBatch         func(engine uintptr, batch uintptr, resultSize uintptr) uintptr
	pScriptEngineVerifyScriptBatchParallel func(engine uintptr, batch uintptr, numThreads int32, resultSize uintptr) uintptr
	pScriptEngineGetSigOpCount             func(engine uintptr, txPtr uintptr, txLen int32, utxoPtr uintptr, utxoLen int32, blockHeight int32, errStr uintptr) uint64
	pScriptEngineCalculateFlags            func(engine uintptr, utxoHeight int32, blockHeight int32, consensus bool) uint32
)

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	purego.RegisterLibFunc(&pScriptEngineCreate, lib, "ScriptEngine_Create")
	purego.RegisterLibFunc(&pScriptEngineDestroy, lib, "ScriptEngine_Destroy")
	purego.RegisterLibFunc(&pScriptEngineSetMaxOpsPerScriptPolicy, lib, "ScriptEngine_SetMaxOpsPerScriptPolicy")
	purego.RegisterLibFunc(&pScriptEngineSetMaxScriptNumLengthPolicy, lib, "ScriptEngine_SetMaxScriptNumLengthPolicy")
	purego.RegisterLibFunc(&pScriptEngineSetMaxScriptSizePolicy, lib, "ScriptEngine_SetMaxScriptSizePolicy")
	purego.RegisterLibFunc(&pScriptEngineSetMaxPubKeysPerMultiSig, lib, "ScriptEngine_SetMaxPubKeysPerMultiSigPolicy")
	purego.RegisterLibFunc(&pScriptEngineSetMaxStackMemoryUsage, lib, "ScriptEngine_SetMaxStackMemoryUsage")
	purego.RegisterLibFunc(&pScriptEngineSetGenesisActivationHeight, lib, "ScriptEngine_SetGenesisActivationHeight")
	purego.RegisterLibFunc(&pScriptEngineSetChronicleActivationHeight, lib, "ScriptEngine_SetChronicleActivationHeight")
	purego.RegisterLibFunc(&pScriptEngineSetGenesisGracefulPeriod, lib, "ScriptEngine_SetGenesisGracefulPeriod")
	purego.RegisterLibFunc(&pScriptEngineSetChronicleGracefulPeriod, lib, "ScriptEngine_SetChronicleGracefulPeriod")
	purego.RegisterLibFunc(&pScriptEngineSetMaxTxSizePolicy, lib, "ScriptEngine_SetMaxTxSizePolicy")
	purego.RegisterLibFunc(&pScriptEngineSetDataCarrierSize, lib, "ScriptEngine_SetDataCarrierSize")
	purego.RegisterLibFunc(&pScriptEngineSetDataCarrier, lib, "ScriptEngine_SetDataCarrier")
	purego.RegisterLibFunc(&pScriptEngineSetAcceptNonStandardOutput, lib, "ScriptEngine_SetAcceptNonStandardOutput")
	purego.RegisterLibFunc(&pScriptEngineSetRequireStandard, lib, "ScriptEngine_SetRequireStandard")
	purego.RegisterLibFunc(&pScriptEngineSetPermitBareMultisig, lib, "ScriptEngine_SetPermitBareMultisig")
	purego.RegisterLibFunc(&pScriptEngineResetDefault, lib, "ScriptEngine_ResetDefault")

	purego.RegisterLibFunc(&pScriptEngineGetMaxOpsPerScript, lib, "ScriptEngine_GetMaxOpsPerScript")
	purego.RegisterLibFunc(&pScriptEngineGetMaxScriptNumLength, lib, "ScriptEngine_GetMaxScriptNumLength")
	purego.RegisterLibFunc(&pScriptEngineGetMaxScriptSize, lib, "ScriptEngine_GetMaxScriptSize")
	purego.RegisterLibFunc(&pScriptEngineGetMaxPubKeysPerMultiSig, lib, "ScriptEngine_GetMaxPubKeysPerMultiSig")
	purego.RegisterLibFunc(&pScriptEngineGetMaxStackMemoryUsage, lib, "ScriptEngine_GetMaxStackMemoryUsage")
	purego.RegisterLibFunc(&pScriptEngineGetMaxTxSize, lib, "ScriptEngine_GetMaxTxSize")
	purego.RegisterLibFunc(&pScriptEngineGetDataCarrierSize, lib, "ScriptEngine_GetDataCarrierSize")
	purego.RegisterLibFunc(&pScriptEngineGetDataCarrier, lib, "ScriptEngine_GetDataCarrier")
	purego.RegisterLibFunc(&pScriptEngineGetAcceptNonStdOutput, lib, "ScriptEngine_GetAcceptNonStandardOutput")
	purego.RegisterLibFunc(&pScriptEngineGetRequireStandard, lib, "ScriptEngine_GetRequireStandard")
	purego.RegisterLibFunc(&pScriptEngineGetPermitBareMultisig, lib, "ScriptEngine_GetPermitBareMultisig")
	purego.RegisterLibFunc(&pScriptEngineGetGenesisActivationHeight, lib, "ScriptEngine_GetGenesisActivationHeight")
	purego.RegisterLibFunc(&pScriptEngineGetChronicleActivationHeight, lib, "ScriptEngine_GetChronicleActivationHeight")
	purego.RegisterLibFunc(&pScriptEngineGetGenesisGracefulPeriod, lib, "ScriptEngine_GetGenesisGracefulPeriod")
	purego.RegisterLibFunc(&pScriptEngineGetChronicleGracefulPeriod, lib, "ScriptEngine_GetChronicleGracefulPeriod")

	purego.RegisterLibFunc(&pScriptEngineVerifyScript, lib, "ScriptEngine_VerifyScript")
	purego.RegisterLibFunc(&pScriptEngineVerifyScriptWithCustomFlags, lib, "ScriptEngine_VerifyScriptWithCustomFlags")
	purego.RegisterLibFunc(&pScriptEngineVerifyScriptBatch, lib, "ScriptEngine_VerifyScriptBatch")
	purego.RegisterLibFunc(&pScriptEngineVerifyScriptBatchParallel, lib, "ScriptEngine_VerifyScriptBatchParallel")
	purego.RegisterLibFunc(&pScriptEngineGetSigOpCount, lib, "ScriptEngine_GetSigOpCount")
	purego.RegisterLibFunc(&pScriptEngineCalculateFlags, lib, "ScriptEngine_CalculateFlags")
}

// ScriptEngine map to the C++ class ScriptEngine
type ScriptEngine struct {
	ptr uintptr
}

// NewScriptEngine creates a new ScriptEngine, and set the Finalizer to call C++ destructor
func NewScriptEngine(netName string) *ScriptEngine {
	namePtr, nameLen := bdkpurego.CString(netName)
	defer bdkpurego.Free(namePtr)

	ptr := pScriptEngineCreate(namePtr, int32(nameLen))
	if ptr == 0 {
		return nil
	}

	goSE := &ScriptEngine{ptr: ptr}
	runtime.SetFinalizer(goSE, func(obj *ScriptEngine) {
		if obj.ptr != 0 {
			pScriptEngineDestroy(obj.ptr)
			obj.ptr = 0
		}
	})

	return goSE
}

// helper to convert a returned error C string pointer to a Go error
func puregoErrStr(ptr uintptr) error {
	if ptr == 0 {
		return nil
	}
	s := bdkpurego.GoString(ptr)
	bdkpurego.Free(ptr)
	return errors.New(s)
}

func (se *ScriptEngine) GetSigOpCount(extendedTX []byte, utxoHeights []int32, blockHeight int32) (uint64, error) {
	txPtr := bdkpurego.SliceDataPtr(extendedTX)
	utxoPtr := bdkpurego.SliceDataPtr(utxoHeights)

	var errPtr uintptr
	sigOpCount := pScriptEngineGetSigOpCount(se.ptr, txPtr, int32(len(extendedTX)), utxoPtr, int32(len(utxoHeights)), blockHeight, uintptr(unsafe.Pointer(&errPtr)))

	if errPtr != 0 {
		errStr := bdkpurego.GoString(errPtr)
		bdkpurego.Free(errPtr)
		return sigOpCount, errors.New(errStr)
	}
	return sigOpCount, nil
}

func (se *ScriptEngine) CalculateFlags(utxoHeight int32, blockHeight int32, consensus bool) uint32 {
	return pScriptEngineCalculateFlags(se.ptr, utxoHeight, blockHeight, consensus)
}

func (se *ScriptEngine) VerifyScript(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) ScriptError {
	txPtr := bdkpurego.SliceDataPtr(extendedTX)
	utxoPtr := bdkpurego.SliceDataPtr(utxoHeights)

	errCode := int(pScriptEngineVerifyScript(se.ptr, txPtr, int32(len(extendedTX)), utxoPtr, int32(len(utxoHeights)), blockHeight, consensus))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return NewScriptError(ScriptErrorCode(errCode))
}

func (se *ScriptEngine) VerifyScriptWithCustomFlags(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool, customFlags []uint32) ScriptError {
	txPtr := bdkpurego.SliceDataPtr(extendedTX)
	utxoPtr := bdkpurego.SliceDataPtr(utxoHeights)
	flagsPtr := bdkpurego.SliceDataPtr(customFlags)

	errCode := int(pScriptEngineVerifyScriptWithCustomFlags(se.ptr, txPtr, int32(len(extendedTX)), utxoPtr, int32(len(utxoHeights)), blockHeight, consensus, flagsPtr, int32(len(customFlags))))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return NewScriptError(ScriptErrorCode(errCode))
}

func (se *ScriptEngine) SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn int64) error {
	return puregoErrStr(pScriptEngineSetMaxOpsPerScriptPolicy(se.ptr, maxOpsPerScriptPolicyIn))
}

func (se *ScriptEngine) SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn int64) error {
	return puregoErrStr(pScriptEngineSetMaxScriptNumLengthPolicy(se.ptr, maxScriptNumLengthIn))
}

func (se *ScriptEngine) SetMaxScriptSizePolicy(maxScriptSizePolicyIn int64) error {
	return puregoErrStr(pScriptEngineSetMaxScriptSizePolicy(se.ptr, maxScriptSizePolicyIn))
}

func (se *ScriptEngine) SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn int64) error {
	return puregoErrStr(pScriptEngineSetMaxPubKeysPerMultiSig(se.ptr, maxPubKeysPerMultiSigIn))
}

func (se *ScriptEngine) SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn int64, maxStackMemoryUsagePolicyIn int64) error {
	return puregoErrStr(pScriptEngineSetMaxStackMemoryUsage(se.ptr, maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn))
}

func (se *ScriptEngine) SetGenesisActivationHeight(genesisActivationHeightIn int32) error {
	return puregoErrStr(pScriptEngineSetGenesisActivationHeight(se.ptr, genesisActivationHeightIn))
}

func (se *ScriptEngine) SetChronicleActivationHeight(chronicleActivationHeightIn int32) error {
	return puregoErrStr(pScriptEngineSetChronicleActivationHeight(se.ptr, chronicleActivationHeightIn))
}

func (se *ScriptEngine) SetGenesisGracefulPeriod(genesisGracefulPeriodIn int64) error {
	return puregoErrStr(pScriptEngineSetGenesisGracefulPeriod(se.ptr, genesisGracefulPeriodIn))
}

func (se *ScriptEngine) SetChronicleGracefulPeriod(chronicleGracefulPeriodIn int64) error {
	return puregoErrStr(pScriptEngineSetChronicleGracefulPeriod(se.ptr, chronicleGracefulPeriodIn))
}

func (se *ScriptEngine) SetMaxTxSizePolicy(value int64) error {
	return puregoErrStr(pScriptEngineSetMaxTxSizePolicy(se.ptr, value))
}

func (se *ScriptEngine) SetDataCarrierSize(dataCarrierSize uint64) {
	pScriptEngineSetDataCarrierSize(se.ptr, dataCarrierSize)
}

func (se *ScriptEngine) SetDataCarrier(dataCarrier bool) {
	pScriptEngineSetDataCarrier(se.ptr, dataCarrier)
}

func (se *ScriptEngine) SetAcceptNonStandardOutput(accept bool) {
	pScriptEngineSetAcceptNonStandardOutput(se.ptr, accept)
}

func (se *ScriptEngine) SetRequireStandard(require bool) {
	pScriptEngineSetRequireStandard(se.ptr, require)
}

func (se *ScriptEngine) SetPermitBareMultisig(permit bool) {
	pScriptEngineSetPermitBareMultisig(se.ptr, permit)
}

func (se *ScriptEngine) ResetDefault() {
	pScriptEngineResetDefault(se.ptr)
}

func (se *ScriptEngine) GetMaxOpsPerScript(isGenesisEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxOpsPerScript(se.ptr, isGenesisEnabled, isConsensus)
}

func (se *ScriptEngine) GetMaxScriptNumLength(isGenesisEnabled, isChronicleEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxScriptNumLength(se.ptr, isGenesisEnabled, isChronicleEnabled, isConsensus)
}

func (se *ScriptEngine) GetMaxScriptSize(isGenesisEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxScriptSize(se.ptr, isGenesisEnabled, isConsensus)
}

func (se *ScriptEngine) GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxPubKeysPerMultiSig(se.ptr, isGenesisEnabled, isConsensus)
}

func (se *ScriptEngine) GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxStackMemoryUsage(se.ptr, isGenesisEnabled, isConsensus)
}

func (se *ScriptEngine) GetGenesisActivationHeight() int32 {
	return pScriptEngineGetGenesisActivationHeight(se.ptr)
}

func (se *ScriptEngine) GetChronicleActivationHeight() int32 {
	return pScriptEngineGetChronicleActivationHeight(se.ptr)
}

func (se *ScriptEngine) GetGenesisGracefulPeriod() uint64 {
	return pScriptEngineGetGenesisGracefulPeriod(se.ptr)
}

func (se *ScriptEngine) GetChronicleGracefulPeriod() uint64 {
	return pScriptEngineGetChronicleGracefulPeriod(se.ptr)
}

func (se *ScriptEngine) GetMaxTxSize(isGenesisEnabled, isChronicleEnabled, isConsensus bool) uint64 {
	return pScriptEngineGetMaxTxSize(se.ptr, isGenesisEnabled, isChronicleEnabled, isConsensus)
}

func (se *ScriptEngine) GetDataCarrierSize() uint64 {
	return pScriptEngineGetDataCarrierSize(se.ptr)
}

func (se *ScriptEngine) GetDataCarrier() bool {
	return pScriptEngineGetDataCarrier(se.ptr)
}

func (se *ScriptEngine) GetAcceptNonStandardOutput(isGenesisEnabled, isChronicleEnabled bool) bool {
	return pScriptEngineGetAcceptNonStdOutput(se.ptr, isGenesisEnabled, isChronicleEnabled)
}

func (se *ScriptEngine) GetRequireStandard() bool {
	return pScriptEngineGetRequireStandard(se.ptr)
}

func (se *ScriptEngine) GetPermitBareMultisig() bool {
	return pScriptEngineGetPermitBareMultisig(se.ptr)
}

func (se *ScriptEngine) VerifyScriptBatch(batch *VerifyBatch) []ScriptError {
	if batch == nil || batch.ptr == 0 {
		return nil
	}

	var resultSize int32
	resultsPtr := pScriptEngineVerifyScriptBatch(se.ptr, batch.ptr, uintptr(unsafe.Pointer(&resultSize)))

	if resultsPtr == 0 || resultSize == 0 {
		return nil
	}
	defer bdkpurego.Free(resultsPtr)

	size := int(resultSize)
	results := make([]ScriptError, size)
	// go vet flags this but resultsPtr is a C-allocated array from malloc
	cResults := unsafe.Slice((*int32)(unsafe.Pointer(resultsPtr)), size)

	for i := 0; i < size; i++ {
		errCode := int(cResults[i])
		if errCode == int(SCRIPT_ERR_OK) {
			results[i] = nil
		} else {
			results[i] = NewScriptError(ScriptErrorCode(errCode))
		}
	}

	return results
}

// VerifyScriptBatchParallel processes a batch of script verifications in parallel on the C++ side
// numThreads controls concurrency: 0 means use hardware_concurrency (all available cores)
func (se *ScriptEngine) VerifyScriptBatchParallel(batch *VerifyBatch, numThreads int) []ScriptError {
	if batch == nil || batch.ptr == 0 {
		return nil
	}

	var resultSize int32
	resultsPtr := pScriptEngineVerifyScriptBatchParallel(se.ptr, batch.ptr, int32(numThreads), uintptr(unsafe.Pointer(&resultSize)))

	if resultsPtr == 0 || resultSize == 0 {
		return nil
	}
	defer bdkpurego.Free(resultsPtr)

	size := int(resultSize)
	results := make([]ScriptError, size)
	cResults := unsafe.Slice((*int32)(unsafe.Pointer(resultsPtr)), size)

	for i := 0; i < size; i++ {
		errCode := int(cResults[i])
		if errCode == int(SCRIPT_ERR_OK) {
			results[i] = nil
		} else {
			results[i] = NewScriptError(ScriptErrorCode(errCode))
		}
	}

	return results
}
