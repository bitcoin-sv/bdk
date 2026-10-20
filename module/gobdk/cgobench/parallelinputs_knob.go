//go:build bdk_internal_hooks

package cgobench

// Reaches BDK's test-only parallel input threshold (core/internal), which no
// binding exposes, so a benchmark can compare serial and parallel input
// verification in one process against the same archive. Built only with
// -tags bdk_internal_hooks, against an archive that contains the hook.

/*
#cgo CXXFLAGS: -std=c++20 -I${SRCDIR}/../../../core
#include <stddef.h>
size_t cgobench_set_parallel_input_threshold(size_t minInputs);
*/
import "C"

// setParallelInputThreshold sets the input count from which BDK verifies a
// transaction's inputs on several threads and returns the previous value.
func setParallelInputThreshold(minInputs uint64) uint64 {
	return uint64(C.cgobench_set_parallel_input_threshold(C.size_t(minInputs)))
}
