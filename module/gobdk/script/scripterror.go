package script

import (
	"fmt"
	"unsafe"
)

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
#include <cgo/script_error_cgo.h>
*/
import "C"

// Define a custom type for the enum
type ScriptErrorCode int

// Declare the enum values using iota
const (
	SCRIPT_ERR_OK ScriptErrorCode = iota // In go binding, equivalent of C++ SCRIPT_ERR_OK is error nil in golang
	SCRIPT_ERR_UNKNOWN_ERROR
	SCRIPT_ERR_EVAL_FALSE
	SCRIPT_ERR_OP_RETURN

	/* Max sizes */
	SCRIPT_ERR_SCRIPT_SIZE
	SCRIPT_ERR_PUSH_SIZE
	SCRIPT_ERR_OP_COUNT
	SCRIPT_ERR_STACK_SIZE
	SCRIPT_ERR_SIG_COUNT
	SCRIPT_ERR_PUBKEY_COUNT

	/* Operands checks */
	SCRIPT_ERR_INVALID_OPERAND_SIZE
	SCRIPT_ERR_INVALID_NUMBER_RANGE
	SCRIPT_ERR_IMPOSSIBLE_ENCODING
	SCRIPT_ERR_INVALID_SPLIT_RANGE
	SCRIPT_ERR_SCRIPTNUM_OVERFLOW
	SCRIPT_ERR_SCRIPTNUM_MINENCODE

	/* Failed verify operations */
	SCRIPT_ERR_VERIFY
	SCRIPT_ERR_EQUALVERIFY
	SCRIPT_ERR_CHECKMULTISIGVERIFY
	SCRIPT_ERR_CHECKSIGVERIFY
	SCRIPT_ERR_NUMEQUALVERIFY

	/* Logical/Format/Canonical errors */
	SCRIPT_ERR_BAD_OPCODE
	SCRIPT_ERR_DISABLED_OPCODE
	SCRIPT_ERR_INVALID_STACK_OPERATION
	SCRIPT_ERR_INVALID_ALTSTACK_OPERATION
	SCRIPT_ERR_UNBALANCED_CONDITIONAL

	/* Divisor errors */
	SCRIPT_ERR_DIV_BY_ZERO
	SCRIPT_ERR_MOD_BY_ZERO

	/* CHECKLOCKTIMEVERIFY and CHECKSEQUENCEVERIFY */
	SCRIPT_ERR_NEGATIVE_LOCKTIME
	SCRIPT_ERR_UNSATISFIED_LOCKTIME

	/* Malleability */
	SCRIPT_ERR_SIG_HASHTYPE
	SCRIPT_ERR_SIG_DER
	SCRIPT_ERR_MINIMALDATA
	SCRIPT_ERR_SIG_PUSHONLY
	SCRIPT_ERR_SIG_HIGH_S
	SCRIPT_ERR_SIG_NULLDUMMY
	SCRIPT_ERR_PUBKEYTYPE
	SCRIPT_ERR_CLEANSTACK
	SCRIPT_ERR_MINIMALIF
	SCRIPT_ERR_SIG_NULLFAIL

	/* softfork safeness */
	SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS

	/* misc */
	SCRIPT_ERR_NONCOMPRESSED_PUBKEY

	/* anti replay */
	SCRIPT_ERR_ILLEGAL_FORKID
	SCRIPT_ERR_MUST_USE_FORKID

	SCRIPT_ERR_BIG_INT

	SCRIPT_ERR_INVALID_FLAGS

	// By ocnvention, if an returned error is higher than SCRIPT_ERR_ERROR_COUNT
	// This is an exception being thrown by C++ code
	SCRIPT_ERR_ERROR_COUNT

	SCRIPT_ERR_CGO_EXCEPTION
)

// errorCode2String conversion of ScriptErrorCode to string, using C++ code
func errorCode2String(e ScriptErrorCode) string {
	if e > SCRIPT_ERR_ERROR_COUNT {
		return "Exception thrown from C++ code"
	}

	errCStr := C.cgo_script_error_string(C.int(int(e)))
	defer C.free(unsafe.Pointer(errCStr))
	errStr := C.GoString(errCStr)
	return errStr
}

// ScriptError is the returned result when executing/verifying a script
type ScriptError interface {
	error
	Code() ScriptErrorCode
	SetInternal(err error)
}

// ScriptErrorImpl implement the interface ScriptError
type ScriptErrorImpl struct {
	errCode  ScriptErrorCode `json:"-"`
	internal error           `json:"-"` // Stores the error returned by an external dependency
}

// Code implement the standard error interface.
func (se ScriptErrorImpl) Code() ScriptErrorCode {
	return se.errCode
}

// Error implement the standard error interface.
func (se ScriptErrorImpl) Error() string {
	if se.internal == nil {
		return errorCode2String(se.errCode)
	}
	return fmt.Sprintf("code=%d, message=%v, internal=%v", se.Code, errorCode2String(se.errCode), se.internal)
}

// Unwrap implement the standard error wrapper interface.
func (se ScriptErrorImpl) Unwrap() error {
	return se.internal
}

// SetInternal sets error to ScriptErrorImpl.internal, to be unwrapped later
func (se ScriptErrorImpl) SetInternal(err error) {
	se.internal = err
}