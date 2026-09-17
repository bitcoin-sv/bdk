package script

import (
	"runtime"
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
// An input that cannot cross the C ABI, or a script the C++ side could not produce,
// gives an empty result.
func FromASM(asmStr string) []byte {
	var scriptLen C.uint64_t

	cAsmLen, err := toABILen(len(asmStr))
	if err != nil {
		return []byte{}
	}

	asmPtr := C.CString(asmStr)
	defer C.free(unsafe.Pointer(asmPtr))

	scriptPtr := C.cgo_from_asm_v2(asmPtr, cAsmLen, &scriptLen)
	if scriptPtr == nil {
		return []byte{}
	}
	defer C.free(unsafe.Pointer(scriptPtr))

	// cgo_from_asm_v2 reports the length as uint64_t, so bound it before it becomes a
	// Go slice length. GoBytes is deliberately not used here: it takes a C int and
	// would reintroduce the narrowing on the way back into Go.
	size, err := abiToGoLen(scriptLen)
	if err != nil || size == 0 {
		return []byte{}
	}

	goScript := make([]byte, size)
	copy(goScript, unsafe.Slice((*byte)(unsafe.Pointer(scriptPtr)), size))

	return goScript
}

// ToASM take input as the script binary
// give output as a string of script in assembler language.
// An empty script gives an empty string.
func ToASM(script []byte) string {
	scriptPtr, scriptLen, err := abiBuffer(script)
	if err != nil {
		return ""
	}

	asmStr := C.cgo_to_asm_v2(scriptPtr, scriptLen)
	runtime.KeepAlive(script)
	if asmStr == nil {
		return ""
	}
	defer C.free(unsafe.Pointer(asmStr))

	return C.GoString(asmStr)
}
