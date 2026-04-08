//go:build !cgo || bdk_purego

package script

import (
	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

var (
	pCgoScriptErrorString            func(errCode int32) uintptr
	pScriptEngineCPPScriptErrErrCnt func() int32
)

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	purego.RegisterLibFunc(&pCgoScriptErrorString, lib, "cgo_script_error_string")
	purego.RegisterLibFunc(&pScriptEngineCPPScriptErrErrCnt, lib, "ScriptEngine_CPP_SCRIPT_ERR_ERROR_COUNT")
}

func CPP_SCRIPT_ERR_ERROR_COUNT() int {
	return int(pScriptEngineCPPScriptErrErrCnt())
}

func errorCode2String(e ScriptErrorCode) string {
	if e > SCRIPT_ERR_ERROR_COUNT {
		return "Exception thrown from C++ code"
	}

	// cgo_script_error_string returns a static string - do NOT free it
	errPtr := pCgoScriptErrorString(int32(e))
	return bdkpurego.GoString(errPtr)
}
