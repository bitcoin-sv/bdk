package main


/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/lib -lGoSESDK -lstdc++
#include "interpreter_cgo.h"
*/
import "C"

import (
	"fmt"
)

// Library code ////////////////////////////////////////////////////////////////

// VersionString return the version string in format semver Major.Minor.Patch
func VersionString() string {
	cStr := C.VersionString()
	goStr := C.GoString(cStr)
    return goStr
}

// Library code ////////////////////////////////////////////////////////////////


// main code
func main() {
    fmt.Println("Golang module version : ",VersionString())
}