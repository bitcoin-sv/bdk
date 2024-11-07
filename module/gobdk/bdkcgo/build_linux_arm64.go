package bdkcgo

/*
#cgo CFLAGS: -I./..
#cgo LDFLAGS: -L./../bdkcgo/lib -lGoBDK_linux_aarch64 -lstdc++ -lm

#include <bdkcgo/include/gobdk.h>
#include <bdkcgo/include/asm_cgo.h>
#include <bdkcgo/include/global_scriptconfig.h>
#include <bdkcgo/include/interpreter_cgo.h>
#include <bdkcgo/include/script_error_cgo.h>
#include <bdkcgo/include/version_cgo.h>
*/
import "C"

// BDKCGOInfo hold information of the CGO
const BDKCGOInfo = "CGO BDK Linux ARM64"
