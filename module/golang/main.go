package main


/*
#cgo CFLAGS: -I${SRCDIR}/cgo
#cgo LDFLAGS: -L${SRCDIR}/lib -lGoSESDK -lstdc++
#include "interpreter_cgo.h"
*/
import "C"

func main() {
    C.hello_cgo()
}