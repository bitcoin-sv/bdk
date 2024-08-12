package main


/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/lib -lGoSESDK -lstdc++
#include "interpreter_cgo.h"
*/
import "C"

func main() {
    C.hello_cgo()
}