package cgobench

/*
#cgo CFLAGS: -I${SRCDIR}
#include "noop.h"
*/
import "C"

import "unsafe"

// CallCGoNoOp is the actual Go function that crosses the CGo boundary.
func CallCGoNoOp() {
	C.C_NoOp()
}

// CallGoNoOp is a trivial pure Go function, defined here so it is not inlined
// into the test file, giving a cleaner baseline measurement.
func CallGoNoOp() {
	// Baseline Go function
}

// CallCGoSumBytes crosses the boundary and passes a byte slice.
func CallCGoSumBytes(data []byte) C.longlong {
	if len(data) == 0 {
		return 0
	}
	// CGo will copy the Go slice into C-allocated memory for safety,
	// which adds the data copying overhead.
	return C.C_SumBytes(
		(*C.uchar)(unsafe.Pointer(&data[0])),
		C.int(len(data)),
	)
}
