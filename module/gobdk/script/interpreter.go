package script

// To make this work on local dev
//   Build the full sesdk library, make install it to a specific location. Then set the environment variables
//
//     export BDK_ROOT=/path/to/install/directory
//     export CGO_CFLAGS="-I${BDK_ROOT}/include/cgo"
//     export CGO_LDFLAGS="-L${BDK_ROOT}/bin"
//     export LD_LIBRARY_PATH=${BDK_ROOT}/bin:$LD_LIBRARY_PATH
//
// To make a build inside docker, the same, i.e
//   - Get the docker images that have all the necessary dependencies for C++ build
//   - Build this sesdk libraries, and make install it to a location
//   - Copy the installed files to the release image. To optimize, copy only the neccessary part,
//   - For golang module, copy only the shared library go, and headers in cgo.
// There might be other system shared library that is required for the executable, just copy them
// to the release docker image. Use lld to know which one is missing.

/*
#cgo LDFLAGS: -lGoBDK -lstdc++ -lm
#include "gobdk.h"
*/
import "C"

import (
	"unsafe"
)

// ExecuteNoVerify executes the script without verification
func ExecuteNoVerify(script []byte, consensus bool, flag uint) int {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	return int(C.cgo_execute_no_verify(scriptPtr, C.int(len(script)), C.bool(consensus), C.uint(flag)))
}

// Execute executes the script with verification
func Execute(script []byte, consensus bool, flag uint,
	tx []byte,
	index int,
	amount uint64,
) int {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	txPtr := (*C.char)(unsafe.Pointer(&tx[0]))
	return int(C.cgo_execute(scriptPtr, C.int(len(script)), C.bool(consensus), C.uint(flag), txPtr, C.int(len(tx)), C.int(index), C.ulonglong(amount)))
}

// ScriptVerificationFlags calculates the flags to be used when verifying scripts
// It is calculated based on the locking script and the boolean isPostChronical
// If the node parameter -genesis is set to true, then the argument isPostChronical is false
// Otherwise, isPostChronical is true
func ScriptVerificationFlags(lScript []byte, isChronicle bool) uint {
	lScriptPtr := (*C.char)(unsafe.Pointer(&lScript[0]))
	return uint(C.cgo_script_verification_flags(lScriptPtr, C.int(len(lScript)), C.bool(isChronicle)))
}

// Verify verify the unlocking and locking script
func Verify(uScript []byte, lScript []byte,
	consensus bool, flag uint,
	tx []byte,
	index int,
	amount uint64,
) int {
	uScriptPtr := (*C.char)(unsafe.Pointer(&uScript[0]))
	lScriptPtr := (*C.char)(unsafe.Pointer(&lScript[0]))
	txPtr := (*C.char)(unsafe.Pointer(&tx[0]))
	return int(C.cgo_verify(uScriptPtr, C.int(len(uScript)), lScriptPtr, C.int(len(lScript)), C.bool(consensus), C.uint(flag), txPtr, C.int(len(tx)), C.int(index), C.ulonglong(amount)))
}
