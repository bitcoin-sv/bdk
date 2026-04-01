//go:build !cgo || purego

package cgobench

// CallCGoNoOp is a stub for the purego build — the CGO benchmark is not applicable.
func CallCGoNoOp() {}

// CallGoNoOp is a trivial pure Go function for baseline measurement.
func CallGoNoOp() {}

// CallCGoSumBytes is a stub for the purego build.
func CallCGoSumBytes(data []byte) int64 {
	return 0
}
