package gobdk

/*
#cgo CFLAGS: -I${SRCDIR}
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// /!  Version of Bitcoin SV on which BDK has been built
func BSV_CLIENT_VERSION_MAJOR() int { return int(C.CGO_BSV_CLIENT_VERSION_MAJOR) }

func BSV_CLIENT_VERSION_MINOR() int    { return int(C.CGO_BSV_CLIENT_VERSION_MINOR) }
func BSV_CLIENT_VERSION_REVISION() int { return int(C.CGO_BSV_CLIENT_VERSION_REVISION) }
func BSV_VERSION_STRING() string       { return C.GoString(C.CGO_BSV_VERSION_STRING) }

func BSV_GIT_COMMIT_TAG_OR_BRANCH() string { return C.GoString(C.CGO_BSV_GIT_COMMIT_TAG_OR_BRANCH) }
func BSV_GIT_COMMIT_HASH() string          { return C.GoString(C.CGO_BSV_GIT_COMMIT_HASH) }
func BSV_GIT_COMMIT_DATETIME() string      { return C.GoString(C.CGO_BSV_GIT_COMMIT_DATETIME) }

// // /!  Global version of Bitcoin Development Kit
func BDK_VERSION_MAJOR() int     { return int(C.CGO_BDK_VERSION_MAJOR) }
func BDK_VERSION_MINOR() int     { return int(C.CGO_BDK_VERSION_MINOR) }
func BDK_VERSION_PATCH() int     { return int(C.CGO_BDK_VERSION_PATCH) }
func BDK_VERSION_STRING() string { return C.GoString(C.CGO_BDK_VERSION_STRING) }

func SOURCE_GIT_COMMIT_TAG_OR_BRANCH() string {
	return C.GoString(C.CGO_SOURCE_GIT_COMMIT_TAG_OR_BRANCH)
}
func SOURCE_GIT_COMMIT_HASH() string     { return C.GoString(C.CGO_SOURCE_GIT_COMMIT_HASH) }
func SOURCE_GIT_COMMIT_DATETIME() string { return C.GoString(C.CGO_SOURCE_GIT_COMMIT_DATETIME) }

// BDK_BUILD_DATETIME_UTC returns the UTC time of the configure that first saw the
// current source revision (the BDK and bitcoin-sv commits, including their "_dirty"
// state at that configure), or SOURCE_DATE_EPOCH when the build set it. The "_dirty"
// suffix on SOURCE_GIT_COMMIT_HASH reflects the tree at the last configure, not edits
// made after it.
func BDK_BUILD_DATETIME_UTC() string { return C.GoString(C.CGO_BDK_BUILD_DATETIME_UTC) }

// // /!  Version of Golang Bitcoin Development Kit
func BDK_GOLANG_VERSION_MAJOR() int     { return int(C.BDK_GOLANG_VERSION_MAJOR) }
func BDK_GOLANG_VERSION_MINOR() int     { return int(C.BDK_GOLANG_VERSION_MINOR) }
func BDK_GOLANG_VERSION_PATCH() int     { return int(C.BDK_GOLANG_VERSION_PATCH) }
func BDK_GOLANG_VERSION_STRING() string { return C.GoString(C.BDK_GOLANG_VERSION_STRING) }

// SHA256_BACKEND returns the SHA-256 transforms BDK hashes with on this CPU, e.g.
// "shani,shani(2way)", "sse4", "arm_shani,arm_shani(2way)" or "standard", and
// "standard(forced)" when BDK_SHA256_FORCE_SCALAR is set. The first call, or the
// first TxValidator, makes the selection, so calling it at startup before any
// validation is safe and reports what validation will use.
func SHA256_BACKEND() string { return C.GoString(C.cgo_sha256_backend()) }
