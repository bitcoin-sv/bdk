package script

import (
	"fmt"
	"unsafe"

	goconfig "github.com/bitcoin-sv/bdk/module/gobdk/config"
)

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
#include <stdlib.h>
#include "gobdk.h"
*/
import "C"

// SetGlobalScriptConfig set config globally for script operations
func SetGlobalScriptConfig(config goconfig.ScriptConfig) error {

	errCStr := C.SetGlobalScriptConfig(
		C.ulonglong(config.MaxOpsPerScriptPolicy),
		C.ulonglong(config.MaxScriptNumLengthPolicy),
		C.ulonglong(config.MaxScriptSizePolicy),
		C.ulonglong(config.MaxPubKeysPerMultiSig),
		C.ulonglong(config.MaxStackMemoryUsageConsensus),
		C.ulonglong(config.MaxStackMemoryUsagePolicy),
	)

	errGoStr := C.GoString(errCStr)
	C.free(unsafe.Pointer(errCStr))

	if len(errGoStr) < 1 {
		return nil
	}

	return fmt.Errorf("error while setting global config for script %v", errGoStr)
}
