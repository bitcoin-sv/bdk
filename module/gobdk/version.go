package gobdk


/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/lib -lGoBDK -lstdc++
#include "interpreter_cgo.h"
*/
import "C"

// VersionString return the version string in format semver Major.Minor.Patch
func VersionString() string {
	cStr := C.VersionString()
	goStr := C.GoString(cStr)
    return goStr
}