package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"math"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// abiMaxLength is the largest buffer length the C ABI can carry. The boundary type
// is uint64_t, so every non-negative Go length fits by construction today; the
// constant is named so the guard below keeps its meaning if that type ever narrows.
const abiMaxLength = uint64(math.MaxUint64)

// CheckBufferLength reports whether a Go length can cross the C ABI. Exported so a
// consumer can pre-check without making a call.
//
// It applies no size ceiling. Any ceiling could reject a consensus-valid spend: the
// extended representation of a transaction repeats every previous locking script, and
// nothing in consensus bounds that aggregate, so a limit here would become a de-facto
// consensus rule no other implementation shares.
func CheckBufferLength(n int) error {
	if n < 0 {
		return NewABIError(ABI_ERR_LENGTH_NEGATIVE)
	}
	if uint64(n) > abiMaxLength {
		return NewABIError(ABI_ERR_LENGTH_NOT_REPRESENTABLE)
	}
	return nil
}

// toABILen converts a checked length to the ABI length type.
func toABILen(n int) (C.uint64_t, error) {
	if err := CheckBufferLength(n); err != nil {
		return 0, err
	}
	return C.uint64_t(n), nil
}

// requireBuffer rejects a positive length described by a nil data pointer.
//
// For any slice Go itself can produce this is unreachable: a slice with a non-zero
// length always has a non-nil backing pointer, and unsafe.Slice refuses to build a
// counter-example. The guard is kept because it is the Go-side statement of the same
// rule the C++ boundary enforces (ABI_ERR_NULL_BUFFER), so the two sides cannot drift,
// and because the pointer is obtained with unsafe.SliceData rather than by indexing —
// nothing above it re-derives the pointer, so the check is what actually decides.
func requireBuffer(nonNil bool, n int) error {
	if n > 0 && !nonNil {
		return NewABIError(ABI_ERR_NULL_BUFFER)
	}
	return nil
}

// abiBuffer converts a Go byte slice into the (pointer, length) pair the C ABI takes.
// The error is returned before any C call, so nothing is allocated on the C++ heap for
// an argument that is rejected here.
//
// The pointer is taken with unsafe.SliceData, which reads the slice header rather than
// indexing the slice, so no bounds check stands between the caller and requireBuffer.
// A zero length is deliberately paired with a nil pointer: that is the empty-buffer
// shape the C++ guards and the shim tests expect.
func abiBuffer(data []byte) (*C.char, C.uint64_t, error) {
	n := len(data)
	cLen, err := toABILen(n)
	if err != nil {
		return nil, 0, err
	}

	var ptr *C.char
	if n > 0 {
		ptr = (*C.char)(unsafe.Pointer(unsafe.SliceData(data)))
	}
	if err := requireBuffer(ptr != nil, n); err != nil {
		return nil, 0, err
	}

	return ptr, cLen, nil
}

// abiInt32Buffer is abiBuffer for the UTXO-heights array.
func abiInt32Buffer(data []int32) (*C.int32_t, C.uint64_t, error) {
	n := len(data)
	cLen, err := toABILen(n)
	if err != nil {
		return nil, 0, err
	}

	var ptr *C.int32_t
	if n > 0 {
		ptr = (*C.int32_t)(unsafe.Pointer(unsafe.SliceData(data)))
	}
	if err := requireBuffer(ptr != nil, n); err != nil {
		return nil, 0, err
	}

	return ptr, cLen, nil
}

// abiUint32Buffer is abiBuffer for the custom-flags array.
func abiUint32Buffer(data []uint32) (*C.uint32_t, C.uint64_t, error) {
	n := len(data)
	cLen, err := toABILen(n)
	if err != nil {
		return nil, 0, err
	}

	var ptr *C.uint32_t
	if n > 0 {
		ptr = (*C.uint32_t)(unsafe.Pointer(unsafe.SliceData(data)))
	}
	if err := requireBuffer(ptr != nil, n); err != nil {
		return nil, 0, err
	}

	return ptr, cLen, nil
}

// abiToGoLen converts a length reported back by the C ABI into a Go int, rejecting
// what a Go int cannot hold rather than wrapping it.
func abiToGoLen(n C.uint64_t) (int, error) {
	if uint64(n) > uint64(math.MaxInt) {
		return 0, NewABIError(ABI_ERR_LENGTH_NOT_REPRESENTABLE)
	}
	return int(n), nil
}
