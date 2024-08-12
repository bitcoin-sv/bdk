package gobdk


/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/lib -lGoSESDK -lsesdk_core -lsecp256k1 -lcrypto -lssl -lboost_filesystem -lboost_thread -lboost_chrono -lboost_program_options -lstdc++ -lm
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