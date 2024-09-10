package script

// To make this work on local dev
//   Build the full bdk library, make install it to a specific location. Then set the environment variables
//
//     export BDK_ROOT=/path/to/install/directory
//     export CGO_CFLAGS="-I${BDK_ROOT}/include/cgo"
//     export CGO_LDFLAGS="-L${BDK_ROOT}/bin"
//     export LD_LIBRARY_PATH=${BDK_ROOT}/bin:$LD_LIBRARY_PATH
//
// To make a build inside docker, the same, i.e
//   - Get the docker images that have all the necessary dependencies for C++ build
//   - Build this bdk libraries, and make install it to a location
//   - Copy the installed files to the release image. To optimize, copy only the neccessary part,
//   - For golang module, copy only the shared library go, and headers in cgo.
// There might be other system shared library that is required for the executable, just copy them
// to the release docker image. Use lld to know which one is missing.

/*
#cgo LDFLAGS: -lGoBDK -lstdc++ -lm
#include <stdlib.h>
#include <cgo/gobdk.h>
*/
import "C"

import (
	"unsafe"
)

// FromASM take input as a string of script in assembler language
// give output as the script binary.
func FromASM(asmStr string) []byte {
	var scriptLen C.int

	asmPtr := C.CString(asmStr)
	defer C.free(unsafe.Pointer(asmPtr))
	asmLen := len(asmStr)
	scriptPtr := C.cgo_from_asm(asmPtr, C.int(asmLen), &scriptLen)
	defer C.free(unsafe.Pointer(scriptPtr))

	goScript := C.GoBytes(unsafe.Pointer(scriptPtr), scriptLen)

	return goScript
}

// ToASM take input as the script binary
// give output as a string of script in assembler language.
func ToASM(script []byte) string {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	scriptLen := len(script)
	asmStr := C.cgo_to_asm(scriptPtr, C.int(scriptLen))
	defer C.free(unsafe.Pointer(asmStr))
	asmGoStr := C.GoString(asmStr)
	return asmGoStr
}
