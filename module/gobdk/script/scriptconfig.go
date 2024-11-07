package script

/*
#cgo CFLAGS: -I./..
#include <stdlib.h>
#include <bdkcgo/include/gobdk.h>
*/
import "C"

import (
	"fmt"
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"

	goconfig "github.com/bitcoin-sv/bdk/module/gobdk/config"
)

// SetGlobalScriptConfig set config globally for script operations
func SetGlobalScriptConfig(config goconfig.ScriptConfig) error {

	networkCStr := C.CString(config.ChainNetwork)
	defer C.free(unsafe.Pointer(networkCStr))

	errScriptConfigCStr := C.CgoSetGlobalScriptConfig(
		networkCStr,
		C.int64_t(config.MaxOpsPerScriptPolicy),
		C.int64_t(config.MaxScriptNumLengthPolicy),
		C.int64_t(config.MaxScriptSizePolicy),
		C.int64_t(config.MaxPubKeysPerMultiSig),
		C.int64_t(config.MaxStackMemoryUsageConsensus),
		C.int64_t(config.MaxStackMemoryUsagePolicy),
	)

	errScriptConfig := C.GoString(errScriptConfigCStr)
	defer C.free(unsafe.Pointer(errScriptConfigCStr))

	if len(errScriptConfig) > 0 {
		return fmt.Errorf("error while setting global config for script engine %v", errScriptConfig)
	}

	return nil
}
