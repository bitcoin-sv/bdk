//go:build cgo && !purego

package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

func CPP_SCRIPT_ERR_ERROR_COUNT() int {
	return int(C.ScriptEngine_CPP_SCRIPT_ERR_ERROR_COUNT())
}

func errorCode2String(e ScriptErrorCode) string {
	if e > SCRIPT_ERR_ERROR_COUNT {
		return "Exception thrown from C++ code"
	}

	errCStr := C.cgo_script_error_string(C.int(int(e)))
	errStr := C.GoString(errCStr)
	return errStr
}
