package gobdk

// To make this work on local dev
//   Build the full sesdk library, make install it to a specific location. Then set the environment variables
//
//     export SESDK_ROOT=/path/to/install/directory
//     export CGO_CFLAGS="-I${SESDK_ROOT}/include/cgo"
//     export CGO_LDFLAGS="-L${SESDK_ROOT}/bin"
//     export LD_LIBRARY_PATH=${SESDK_ROOT}/bin:$LD_LIBRARY_PATH
//
// To make a build inside docker, the same, i.e
//   - Get the docker images that have all the necessary dependencies for C++ build
//   - Build this sesdk libraries, and make install it to a location
//   - Copy the installed files to the release image. To optimize, copy only the neccessary part,
//   - For golang module, copy only the shared library go, and headers in cgo.
// There might be other system shared library that is required for the executable, just copy them
// to the release docker image. Use lld to know which one is missing.

/*
#cgo LDFLAGS: -lGoSESDK -lstdc++ -lm
#include "interpreter_cgo.h"
*/
import "C"

import (
	"unsafe"
)

// Execute execute script
func Execute(script []byte, consensus bool, flag uint) int {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	return int( C.cgo_execute(scriptPtr, C.int(len(script)), C.bool(consensus), C.uint(flag)))
}