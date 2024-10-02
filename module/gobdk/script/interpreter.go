package script

// To make this work on local dev
//   Build the full bdk library, make install it to a specific location. Then set the environment variables
//
//     export BDK_INSTALL_ROOT=/path/to/install/directory
//     export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin"
//     export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include/core -I${BDK_INSTALL_ROOT}/include"
//     export LD_LIBRARY_PATH="${BDK_INSTALL_ROOT}/bin:${LD_LIBRARY_PATH}"
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
#include <cgo/gobdk.h>
#include <script/script_flags.h>
*/
import "C"

import (
	"errors"
	"unsafe"
)

// ScriptVerificationFlags calculates the flags to be used when verifying scripts
// It is calculated based on the locking script and the boolean isPostChronical
// If the node parameter -genesis is set to true, then the argument isPostChronical is false
// Otherwise, isPostChronical is true
//
// By convention, if the returned flag is greater than SCRIPT_FLAG_LAST in C/C++ code
// It means an exception has been thrown and handled from the C/C++ layer
func ScriptVerificationFlags(lScript []byte, isChronicle bool) (uint32, error) {
	lScriptPtr := (*C.char)(unsafe.Pointer(&lScript[0]))

	cgoFlags := uint32(C.cgo_script_verification_flags(lScriptPtr, C.int(len(lScript)), C.bool(isChronicle)))
	if cgoFlags > C.SCRIPT_FLAG_LAST {
		return cgoFlags, errors.New("CGO EXCEPTION : Exception has been thrown and handled in C/C++ layer")
	}
	return cgoFlags, nil
}

// ScriptVerificationFlags calculates the flags to be used when verifying scripts
// It is calculated based on the locking script and current block height
//
// By convention, if the returned flag is greater than SCRIPT_FLAG_LAST in C/C++ code
// It means an exception has been thrown and handled from the C/C++ layer
func ScriptVerificationFlagsV2(lScript []byte, blockHeight uint32) (uint32, error) {
	lScriptPtr := (*C.char)(unsafe.Pointer(&lScript[0]))

	cgoFlags := uint32(C.cgo_script_verification_flags_v2(lScriptPtr, C.int(len(lScript)), C.int32_t(blockHeight)))
	if cgoFlags > C.SCRIPT_FLAG_LAST {
		return cgoFlags, errors.New("CGO EXCEPTION : Exception has been thrown and handled in C/C++ layer")
	}
	return cgoFlags, nil
}

// ExecuteNoVerify executes the script without verification
func ExecuteNoVerify(script []byte, consensus bool, flag uint) ScriptError {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	errCode := int(C.cgo_execute_no_verify(scriptPtr, C.int(len(script)), C.bool(consensus), C.uint(flag)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return ScriptErrorImpl{errCode: ScriptErrorCode(errCode)}
}

// Execute executes the script with verification
func Execute(script []byte, consensus bool, flag uint,
	tx []byte,
	index int,
	amount uint64,
) ScriptError {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	txPtr := (*C.char)(unsafe.Pointer(&tx[0]))
	errCode := int(C.cgo_execute(scriptPtr, C.int(len(script)), C.bool(consensus), C.uint(flag), txPtr, C.int(len(tx)), C.int(index), C.ulonglong(amount)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return ScriptErrorImpl{errCode: ScriptErrorCode(errCode)}
}

// Verify verify the unlocking and locking script
func Verify(uScript []byte, lScript []byte,
	consensus bool, flag uint,
	tx []byte,
	index int,
	amount uint64,
) ScriptError {
	uScriptPtr := (*C.char)(unsafe.Pointer(&uScript[0]))
	lScriptPtr := (*C.char)(unsafe.Pointer(&lScript[0]))
	txPtr := (*C.char)(unsafe.Pointer(&tx[0]))

	errCode := int(C.cgo_verify(uScriptPtr, C.int(len(uScript)), lScriptPtr, C.int(len(lScript)), C.bool(consensus), C.uint(flag), txPtr, C.int(len(tx)), C.int(index), C.ulonglong(amount)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return ScriptErrorImpl{errCode: ScriptErrorCode(errCode)}
}
