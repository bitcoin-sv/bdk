package gobdk

/*
#cgo CFLAGS: -I${SRCDIR}/cgo/include
#cgo LDFLAGS: -L${SRCDIR}/cgo/lib -lGoBDK -lstdc++
#include "version_cgo.h"
*/
import "C"

// /!  Version of Bitcoin SV on which BDK has been built
func BSV_CLIENT_VERSION_MAJOR() int    { return int(C.BSV_CLIENT_VERSION_MAJOR) }
func BSV_CLIENT_VERSION_MINOR() int    { return int(C.BSV_CLIENT_VERSION_MINOR) }
func BSV_CLIENT_VERSION_REVISION() int { return int(C.BSV_CLIENT_VERSION_REVISION) }
func BSV_VERSION_STRING() string       { return C.GoString(C.BSV_VERSION_STRING) }

func BSV_GIT_COMMIT_TAG_OR_BRANCH() string { return C.GoString(C.BSV_GIT_COMMIT_TAG_OR_BRANCH) }
func BSV_GIT_COMMIT_HASH() string          { return C.GoString(C.BSV_GIT_COMMIT_HASH) }
func BSV_GIT_COMMIT_DATETIME() string      { return C.GoString(C.BSV_GIT_COMMIT_DATETIME) }

// // /!  Global version of Bitcoin Development Kit
func BDK_VERSION_MAJOR() int     { return int(C.BDK_VERSION_MAJOR) }
func BDK_VERSION_MINOR() int     { return int(C.BDK_VERSION_MINOR) }
func BDK_VERSION_PATCH() int     { return int(C.BDK_VERSION_PATCH) }
func BDK_VERSION_STRING() string { return C.GoString(C.BDK_VERSION_STRING) }

func SOURCE_GIT_COMMIT_TAG_OR_BRANCH() string { return C.GoString(C.SOURCE_GIT_COMMIT_TAG_OR_BRANCH) }
func SOURCE_GIT_COMMIT_HASH() string          { return C.GoString(C.SOURCE_GIT_COMMIT_HASH) }
func SOURCE_GIT_COMMIT_DATETIME() string      { return C.GoString(C.SOURCE_GIT_COMMIT_DATETIME) }
func BDK_BUILD_DATETIME_UTC() string          { return C.GoString(C.BDK_BUILD_DATETIME_UTC) }

// // /!  Version of Golang Bitcoin Development Kit
func BDK_GOLANG_VERSION_MAJOR() int     { return int(C.BDK_GOLANG_VERSION_MAJOR) }
func BDK_GOLANG_VERSION_MINOR() int     { return int(C.BDK_GOLANG_VERSION_MINOR) }
func BDK_GOLANG_VERSION_PATCH() int     { return int(C.BDK_GOLANG_VERSION_PATCH) }
func BDK_GOLANG_VERSION_STRING() string { return C.GoString(C.BDK_GOLANG_VERSION_STRING) }
