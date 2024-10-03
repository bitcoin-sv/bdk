package script

import (
	"fmt"
	"unsafe"

	goconfig "github.com/bitcoin-sv/bdk/module/gobdk/config"
)

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
#include <stdlib.h>
#include <cgo/gobdk.h>
*/
import "C"

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
