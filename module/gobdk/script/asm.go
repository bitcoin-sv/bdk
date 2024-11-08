package script

import (
	"unsafe"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <stdlib.h>
#include <bdkcgo/gobdk.h>
*/
import "C"

// FromASM take input as a string of script in assembler language
// give output as the script binary.
func FromASM(asmStr string) []byte {
	var scriptLen C.int

	asmPtr := C.CString(asmStr)
	defer C.free(unsafe.Pointer(asmPtr))
	asmLen := len(asmStr)
	scriptPtr := C.cgo_from_asm(asmPtr, C.int(asmLen), &scriptLen)
	defer C.free(unsafe.Pointer(scriptPtr))

	goScript := C.GoBytes(unsafe.Pointer(scriptPtr), scriptLen)

	return goScript
}

// ToASM take input as the script binary
// give output as a string of script in assembler language.
func ToASM(script []byte) string {
	scriptPtr := (*C.char)(unsafe.Pointer(&script[0]))
	scriptLen := len(script)
	asmStr := C.cgo_to_asm(scriptPtr, C.int(scriptLen))
	defer C.free(unsafe.Pointer(asmStr))
	asmGoStr := C.GoString(asmStr)
	return asmGoStr
}
