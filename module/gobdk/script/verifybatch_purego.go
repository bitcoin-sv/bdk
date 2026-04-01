//go:build !cgo || purego

package script

import (
	"runtime"

	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

var (
	pVerifyBatchCreate  func() uintptr
	pVerifyBatchDestroy func(batch uintptr)
	pVerifyBatchAdd     func(batch uintptr, txPtr uintptr, txLen int32, utxoPtr uintptr, utxoLen int32, blockHeight int32, consensus bool, flagsPtr uintptr, flagsLen int32)
	pVerifyBatchClear   func(batch uintptr)
	pVerifyBatchSize    func(batch uintptr) int32
	pVerifyBatchEmpty   func(batch uintptr) bool
	pVerifyBatchReserve func(batch uintptr, capacity int32)
)

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	purego.RegisterLibFunc(&pVerifyBatchCreate, lib, "VerifyBatch_Create")
	purego.RegisterLibFunc(&pVerifyBatchDestroy, lib, "VerifyBatch_Destroy")
	purego.RegisterLibFunc(&pVerifyBatchAdd, lib, "VerifyBatch_Add")
	purego.RegisterLibFunc(&pVerifyBatchClear, lib, "VerifyBatch_Clear")
	purego.RegisterLibFunc(&pVerifyBatchSize, lib, "VerifyBatch_Size")
	purego.RegisterLibFunc(&pVerifyBatchEmpty, lib, "VerifyBatch_Empty")
	purego.RegisterLibFunc(&pVerifyBatchReserve, lib, "VerifyBatch_Reserve")
}

// VerifyBatch wraps the C++ VerifyBatch class for batch script verification
type VerifyBatch struct {
	ptr uintptr
}

func NewVerifyBatch(capacity ...int) *VerifyBatch {
	ptr := pVerifyBatchCreate()
	if ptr == 0 {
		return nil
	}

	if len(capacity) > 0 && capacity[0] > 0 {
		pVerifyBatchReserve(ptr, int32(capacity[0]))
	}

	goBatch := &VerifyBatch{ptr: ptr}
	runtime.SetFinalizer(goBatch, func(obj *VerifyBatch) {
		if obj.ptr != 0 {
			pVerifyBatchDestroy(obj.ptr)
			obj.ptr = 0
		}
	})

	return goBatch
}

func (vb *VerifyBatch) Add(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool, customFlags []uint32) {
	txPtr := bdkpurego.SliceDataPtr(extendedTX)
	utxoPtr := bdkpurego.SliceDataPtr(utxoHeights)
	flagsPtr := bdkpurego.SliceDataPtr(customFlags)

	pVerifyBatchAdd(vb.ptr, txPtr, int32(len(extendedTX)), utxoPtr, int32(len(utxoHeights)), blockHeight, consensus, flagsPtr, int32(len(customFlags)))
}

func (vb *VerifyBatch) Clear() {
	pVerifyBatchClear(vb.ptr)
}

func (vb *VerifyBatch) Size() int {
	return int(pVerifyBatchSize(vb.ptr))
}

func (vb *VerifyBatch) Empty() bool {
	return pVerifyBatchEmpty(vb.ptr)
}

func (vb *VerifyBatch) Reserve(capacity int) {
	if capacity > 0 {
		pVerifyBatchReserve(vb.ptr, int32(capacity))
	}
}
