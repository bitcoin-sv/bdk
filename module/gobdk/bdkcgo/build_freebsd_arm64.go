package bdkcgo

// FreeBSD/arm64 archive naming: FreeBSD reports CMAKE_SYSTEM_PROCESSOR as "aarch64",
// so BDK's default merged-archive name would be libGoBDK_freebsd_aarch64.a, which does
// NOT match the -lGoBDK_freebsd_arm64 below (Go's GOARCH is "arm64"). Configure the C++
// build with -DCUSTOM_GOBDK_OS_ARCH=freebsd_arm64 so the archive name matches this link.
// (On amd64 this is unnecessary: FreeBSD already reports "amd64", matching freebsd_amd64.)

/*
#cgo CFLAGS: -I${SRCDIR}/..
#cgo LDFLAGS: -L${SRCDIR}/../bdkcgo -lGoBDK_freebsd_arm64 -lc++ -lm -pthread

#include <bdkcgo/gobdk.h>
#include <bdkcgo/asm_cgo.h>
#include <bdkcgo/script_error_cgo.h>
#include <bdkcgo/txvalidator_cgo.h>
#include <bdkcgo/secp256k1_cloned.h>
#include <bdkcgo/version_cgo.h>
*/
import "C"

// BDKCGOInfo hold information of the CGO
const BDKCGOInfo = "CGO BDK FreeBSD ARM64"
