package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"errors"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// Same definition as in BSV C++ code. We dont want to use the include file from bsv in cgo code
// So we redefine it here, rather including the bsv .h file
const SCRIPT_FLAG_LAST = 1 << 22

// ScriptVerificationFlags calculates the flags to be used when verifying scripts
// It is calculated based on the locking script and the boolean isPostChronical
// If the node parameter -genesis is set to true, then the argument isPostChronical is false
// Otherwise, isPostChronical is true
//
// By convention, if the returned flag is greater than SCRIPT_FLAG_LAST in C/C++ code
// It means an exception has been thrown and handled from the C/C++ layer
func ScriptVerificationFlagsV1(lScript []byte, isChronicle bool) (uint32, error) {
	lenLScript := len(lScript)
	var lScriptPtr *C.char
	if lenLScript > 0 {
		lScriptPtr = (*C.char)(unsafe.Pointer(&lScript[0]))
	}

	cgoFlags := uint32(C.cgo_script_verification_flags_v1(lScriptPtr, C.int(lenLScript), C.bool(isChronicle)))
	if cgoFlags > SCRIPT_FLAG_LAST {
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
	lenLScript := len(lScript)
	var lScriptPtr *C.char
	if lenLScript > 0 {
		lScriptPtr = (*C.char)(unsafe.Pointer(&lScript[0]))
	}

	cgoFlags := uint32(C.cgo_script_verification_flags_v2(lScriptPtr, C.int(lenLScript), C.int32_t(blockHeight)))
	if cgoFlags > SCRIPT_FLAG_LAST {
		return cgoFlags, errors.New("CGO EXCEPTION : Exception has been thrown and handled in C/C++ layer")
	}
	return cgoFlags, nil
}

// ExecuteNoVerify executes the script without verification
func ExecuteNoVerify(script []byte, consensus bool, flag uint) ScriptError {
	lenScript := len(script)
	var scriptPtr *C.char
	if lenScript > 0 {
		scriptPtr = (*C.char)(unsafe.Pointer(&script[0]))
	}

	errCode := int(C.cgo_execute_no_verify(scriptPtr, C.int(lenScript), C.bool(consensus), C.uint(flag)))

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
	lenScript, lenTx := len(script), len(tx)
	var scriptPtr, txPtr *C.char
	if lenScript > 0 {
		scriptPtr = (*C.char)(unsafe.Pointer(&script[0]))
	}

	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&tx[0]))
	} else {
		// If Tx is nil or empty, just execute the script without verification
		return ExecuteNoVerify(script, consensus, flag)
	}

	errCode := int(C.cgo_execute(scriptPtr, C.int(lenScript), C.bool(consensus), C.uint(flag), txPtr, C.int(lenTx), C.int(index), C.ulonglong(amount)))

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

	lenUScript, lenLScript, lenTx := len(uScript), len(lScript), len(tx)
	var uScriptPtr, lScriptPtr, txPtr *C.char

	if lenUScript > 0 {
		uScriptPtr = (*C.char)(unsafe.Pointer(&uScript[0]))
	} else {
		// If Unlocking script is empty, then execute the script with Locking script
		return Execute(lScript, consensus, flag, tx, index, amount)
	}

	if lenLScript > 0 {
		lScriptPtr = (*C.char)(unsafe.Pointer(&lScript[0]))
	} else {
		// If Locking script is empty, then execute the script with Unlocking script
		return Execute(uScript, consensus, flag, tx, index, amount)
	}

	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&tx[0]))
	}

	errCode := int(C.cgo_verify(uScriptPtr, C.int(lenUScript), lScriptPtr, C.int(lenLScript), C.bool(consensus), C.uint(flag), txPtr, C.int(lenTx), C.int(index), C.ulonglong(amount)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return ScriptErrorImpl{errCode: ScriptErrorCode(errCode)}
}

// VerifyExtend verify the entire extended transaction
// It iterates through all the locking and unlocking scripts to verifies
// It return when the first error encountered
func VerifyExtend(extendedTX []byte, blockHeight uint32, consensus bool) ScriptError {

	lenTx := len(extendedTX)
	var txPtr *C.char

	if lenTx > 0 {
		txPtr = (*C.char)(unsafe.Pointer(&extendedTX[0]))
	}

	errCode := int(C.cgo_verify_extend(txPtr, C.int(lenTx), C.int32_t(blockHeight), C.bool(consensus)))

	if errCode == int(SCRIPT_ERR_OK) {
		return nil
	}
	return ScriptErrorImpl{errCode: ScriptErrorCode(errCode)}
}
