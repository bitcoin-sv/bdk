package gobdk


/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/bin -lGoSESDK -lstdc++ -lm -Wl,-rpath,./cgo/bin
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