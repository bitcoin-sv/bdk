package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <stdlib.h>
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"runtime"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// VerifyBatch wraps the C++ VerifyBatch class for batch script verification
type VerifyBatch struct {
	cBatchPtr C.VerifyBatchCGO
}

// NewVerifyBatch creates a new VerifyBatch and sets the Finalizer to call C++ destructor
// Takes an optional capacity parameter to pre-allocate space for batch elements
// Usage:
//   batch := NewVerifyBatch()         // Create batch without pre-allocation
//   batch := NewVerifyBatch(100)      // Create batch with capacity for 100 elements
func NewVerifyBatch(capacity ...int) *VerifyBatch {
	// Create a new C++ VerifyBatch and bind it to the go struct
	goBatch := &VerifyBatch{
		cBatchPtr: C.VerifyBatch_Create(),
	}

	// If C is not able to create the VerifyBatch, then return nil
	if goBatch.cBatchPtr == nil {
		return nil
	}

	// Reserve capacity if provided
	if len(capacity) > 0 && capacity[0] > 0 {
		C.VerifyBatch_Reserve(goBatch.cBatchPtr, C.int(capacity[0]))
	}

	// Set finalizer to delete C++ VerifyBatch when GC collects this struct
	runtime.SetFinalizer(goBatch, func(obj *VerifyBatch) {
		if obj.cBatchPtr != nil {
			C.VerifyBatch_Destroy(obj.cBatchPtr)
			obj.cBatchPtr = nil
		}
	})

	return goBatch
}

// Add adds a verification argument to the batch
// This operation builds up the batch incrementally for later batch processing
//   - extendedTX: The extended transaction binary
//   - utxoHeights: Array of UTXO heights (required to calculate flags)
//   - blockHeight: The current block height
//   - consensus: The consensus parameter
//   - customFlags: Optional custom flags array (can be nil or empty)
//
// All verification arguments are collected and will be processed together
// when the batch is submitted to ScriptEngine for verification
func (vb *VerifyBatch) Add(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool, customFlags []uint32) {
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

	C.VerifyBatch_Add(
		vb.cBatchPtr,
		txPtr, C.int(lenTx),
		utxoPtr, C.int(lenUtxo),
		C.int32_t(blockHeight),
		C.bool(consensus),
		flagsPtr, C.int(lenFlags),
	)
}

// Clear removes all elements from the batch
// The batch can be reused after clearing for a new batch operation
func (vb *VerifyBatch) Clear() {
	C.VerifyBatch_Clear(vb.cBatchPtr)
}

// Size returns the number of elements in the batch
func (vb *VerifyBatch) Size() int {
	return int(C.VerifyBatch_Size(vb.cBatchPtr))
}

// Empty checks if the batch is empty
// Returns true if the batch contains no elements
func (vb *VerifyBatch) Empty() bool {
	return bool(C.VerifyBatch_Empty(vb.cBatchPtr))
}

// Reserve pre-allocates capacity for the specified number of elements
// This is an optimization to avoid multiple reallocations when batch size is known in advance
func (vb *VerifyBatch) Reserve(capacity int) {
	if capacity > 0 {
		C.VerifyBatch_Reserve(vb.cBatchPtr, C.int(capacity))
	}
}
