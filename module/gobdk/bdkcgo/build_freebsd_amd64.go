package bdkcgo

/*
#cgo CFLAGS: -I${SRCDIR}/..
#cgo LDFLAGS: -L${SRCDIR}/../bdkcgo -lGoBDK_freebsd_amd64 -lc++ -lm -pthread

#include <bdkcgo/gobdk.h>
#include <bdkcgo/asm_cgo.h>
#include <bdkcgo/script_error_cgo.h>
#include <bdkcgo/txvalidator_cgo.h>
#include <bdkcgo/secp256k1_cloned.h>
#include <bdkcgo/version_cgo.h>
*/
import "C"

// BDKCGOInfo hold information of the CGO
const BDKCGOInfo = "CGO BDK FreeBSD AMD64"
