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

// ValidateBatch wraps the C++ ValidateBatch class for batch transaction validation.
//
// NOT CONCURRENT-SAFE: All Add calls and the TxValidator.ValidateBatch call that
// executes the batch must happen on the same goroutine (or be externally serialized).
// Internally, Add passes raw pointers into Go-managed slice memory across the CGO
// boundary; concurrent use from another goroutine is a data race.
type ValidateBatch struct {
	cBatchPtr C.ValidateBatchCGO
}

// NewValidateBatch creates a new ValidateBatch and sets the Finalizer to call C++ destructor
// Takes an optional capacity parameter to pre-allocate space for batch elements
// Usage:
//
//	batch := NewValidateBatch()         // Create batch without pre-allocation
//	batch := NewValidateBatch(100)      // Create batch with capacity for 100 elements
func NewValidateBatch(capacity ...int) *ValidateBatch {
	// Create a new C++ ValidateBatch and bind it to the go struct
	goBatch := &ValidateBatch{
		cBatchPtr: C.ValidateBatch_Create(),
	}

	// If C is not able to create the ValidateBatch, then return nil
	if goBatch.cBatchPtr == nil {
		return nil
	}

	// Reserve capacity if provided
	if len(capacity) > 0 && capacity[0] > 0 {
		C.ValidateBatch_Reserve(goBatch.cBatchPtr, C.int(capacity[0]))
	}

	// Set finalizer to delete C++ ValidateBatch when GC collects this struct
	runtime.SetFinalizer(goBatch, func(obj *ValidateBatch) {
		if obj.cBatchPtr != nil {
			C.ValidateBatch_Destroy(obj.cBatchPtr)
			obj.cBatchPtr = nil
		}
	})

	return goBatch
}

// Add adds a validation argument to the batch
// This operation builds up the batch incrementally for later batch processing
//   - extendedTX: The extended transaction binary
//   - utxoHeights: Array of UTXO heights (required to calculate flags)
//   - blockHeight: The current block height
//   - consensus: The consensus parameter
//
// All validation arguments are collected and will be processed together
// when the batch is submitted to TxValidator for validation
func (vb *ValidateBatch) Add(extendedTX []byte, utxoHeights []int32, blockHeight int32, consensus bool) {
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

	C.ValidateBatch_Add(
		vb.cBatchPtr,
		txPtr, C.int(lenTx),
		utxoPtr, C.int(lenUtxo),
		C.int32_t(blockHeight),
		C.bool(consensus),
	)
	runtime.KeepAlive(vb)
}

// Clear removes all elements from the batch
// The batch can be reused after clearing for a new batch operation
func (vb *ValidateBatch) Clear() {
	C.ValidateBatch_Clear(vb.cBatchPtr)
	runtime.KeepAlive(vb)
}

// Size returns the number of elements in the batch
func (vb *ValidateBatch) Size() int {
	result := C.ValidateBatch_Size(vb.cBatchPtr)
	runtime.KeepAlive(vb)
	return int(result)
}

// Empty checks if the batch is empty
// Returns true if the batch contains no elements
func (vb *ValidateBatch) Empty() bool {
	result := C.ValidateBatch_Empty(vb.cBatchPtr)
	runtime.KeepAlive(vb)
	return bool(result)
}

// Reserve pre-allocates capacity for the specified number of elements
// This is an optimization to avoid multiple reallocations when batch size is known in advance
func (vb *ValidateBatch) Reserve(capacity int) {
	if capacity > 0 {
		C.ValidateBatch_Reserve(vb.cBatchPtr, C.int(capacity))
		runtime.KeepAlive(vb)
	}
}
