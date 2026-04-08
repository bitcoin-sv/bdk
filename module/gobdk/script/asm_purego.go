//go:build !cgo || bdk_purego

package script

import (
	"unsafe"

	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

var (
	pCgoFromAsm func(asmPtr uintptr, asmLen int32, scriptLenPtr uintptr) uintptr
	pCgoToAsm   func(scriptPtr uintptr, scriptLen int32) uintptr
)

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	purego.RegisterLibFunc(&pCgoFromAsm, lib, "cgo_from_asm")
	purego.RegisterLibFunc(&pCgoToAsm, lib, "cgo_to_asm")
}

// FromASM take input as a string of script in assembler language
// give output as the script binary.
func FromASM(asmStr string) []byte {
	asmPtr, asmLen := bdkpurego.CString(asmStr)
	defer bdkpurego.Free(asmPtr)

	var scriptLen int32
	scriptPtr := pCgoFromAsm(asmPtr, int32(asmLen), uintptr(unsafe.Pointer(&scriptLen)))
	defer bdkpurego.Free(scriptPtr)

	return bdkpurego.GoBytes(scriptPtr, int(scriptLen))
}

// ToASM take input as the script binary
// give output as a string of script in assembler language.
func ToASM(script []byte) string {
	scriptPtr := bdkpurego.SliceDataPtr(script)
	asmPtr := pCgoToAsm(scriptPtr, int32(len(script)))
	defer bdkpurego.Free(asmPtr)

	return bdkpurego.GoString(asmPtr)
}
