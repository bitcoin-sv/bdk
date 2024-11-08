package bdkcgo

/*
#cgo CFLAGS: -I${SRCDIR}/..
#cgo LDFLAGS: -L${SRCDIR}/../bdkcgo -lGoBDK_linux_x86_64 -lstdc++ -lm

#include <bdkcgo/gobdk.h>
#include <bdkcgo/asm_cgo.h>
#include <bdkcgo/global_scriptconfig.h>
#include <bdkcgo/interpreter_cgo.h>
#include <bdkcgo/script_error_cgo.h>
#include <bdkcgo/version_cgo.h>
*/
import "C"

// BDKCGOInfo hold information of the CGO
const BDKCGOInfo = "CGO BDK Linux AMD64"
