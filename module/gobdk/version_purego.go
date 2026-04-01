//go:build !cgo || purego

package gobdk

import (
	"unsafe"

	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

var (
	vBsvClientVersionMajor    int32
	vBsvClientVersionMinor    int32
	vBsvClientVersionRevision int32
	vBsvVersionString         string
	vBsvGitCommitTagOrBranch  string
	vBsvGitCommitHash         string
	vBsvGitCommitDatetime     string
	vBdkVersionMajor          int32
	vBdkVersionMinor          int32
	vBdkVersionPatch          int32
	vBdkVersionString         string
	vSourceGitCommitTagOrBranch string
	vSourceGitCommitHash        string
	vSourceGitCommitDatetime    string
	vBdkBuildDatetimeUTC        string
	vBdkGolangVersionMajor    int32
	vBdkGolangVersionMinor    int32
	vBdkGolangVersionPatch    int32
	vBdkGolangVersionString   string
)

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	vBsvClientVersionMajor = readInt(lib, "CGO_BSV_CLIENT_VERSION_MAJOR")
	vBsvClientVersionMinor = readInt(lib, "CGO_BSV_CLIENT_VERSION_MINOR")
	vBsvClientVersionRevision = readInt(lib, "CGO_BSV_CLIENT_VERSION_REVISION")
	vBsvVersionString = readConstString(lib, "CGO_BSV_VERSION_STRING")
	vBsvGitCommitTagOrBranch = readConstString(lib, "CGO_BSV_GIT_COMMIT_TAG_OR_BRANCH")
	vBsvGitCommitHash = readConstString(lib, "CGO_BSV_GIT_COMMIT_HASH")
	vBsvGitCommitDatetime = readConstString(lib, "CGO_BSV_GIT_COMMIT_DATETIME")
	vBdkVersionMajor = readInt(lib, "CGO_BDK_VERSION_MAJOR")
	vBdkVersionMinor = readInt(lib, "CGO_BDK_VERSION_MINOR")
	vBdkVersionPatch = readInt(lib, "CGO_BDK_VERSION_PATCH")
	vBdkVersionString = readConstString(lib, "CGO_BDK_VERSION_STRING")
	vSourceGitCommitTagOrBranch = readConstString(lib, "CGO_SOURCE_GIT_COMMIT_TAG_OR_BRANCH")
	vSourceGitCommitHash = readConstString(lib, "CGO_SOURCE_GIT_COMMIT_HASH")
	vSourceGitCommitDatetime = readConstString(lib, "CGO_SOURCE_GIT_COMMIT_DATETIME")
	vBdkBuildDatetimeUTC = readConstString(lib, "CGO_BDK_BUILD_DATETIME_UTC")
	vBdkGolangVersionMajor = readInt(lib, "BDK_GOLANG_VERSION_MAJOR")
	vBdkGolangVersionMinor = readInt(lib, "BDK_GOLANG_VERSION_MINOR")
	vBdkGolangVersionPatch = readInt(lib, "BDK_GOLANG_VERSION_PATCH")
	vBdkGolangVersionString = readConstString(lib, "BDK_GOLANG_VERSION_STRING")
}

// readInt reads an extern const int global variable via Dlsym
func readInt(lib uintptr, name string) int32 {
	addr, err := purego.Dlsym(lib, name)
	if err != nil {
		return 0
	}
	return *(*int32)(addrToPointer(addr))
}

// readConstString reads an extern const char* global variable via Dlsym.
// Dlsym returns the address of the pointer variable; we dereference once to get
// the char* value, then read the C string.
func readConstString(lib uintptr, name string) string {
	addr, err := purego.Dlsym(lib, name)
	if err != nil {
		return ""
	}
	strPtr := *(*uintptr)(addrToPointer(addr))
	return bdkpurego.GoString(strPtr)
}

// addrToPointer converts a Dlsym symbol address to an unsafe.Pointer.
// go vet flags this as "possible misuse of unsafe.Pointer" but Dlsym addresses
// are valid C symbol pointers, not Go-managed memory.
func addrToPointer(addr uintptr) unsafe.Pointer {
	return unsafe.Pointer(addr) //nolint:govet
}

func BSV_CLIENT_VERSION_MAJOR() int    { return int(vBsvClientVersionMajor) }
func BSV_CLIENT_VERSION_MINOR() int    { return int(vBsvClientVersionMinor) }
func BSV_CLIENT_VERSION_REVISION() int { return int(vBsvClientVersionRevision) }
func BSV_VERSION_STRING() string       { return vBsvVersionString }

func BSV_GIT_COMMIT_TAG_OR_BRANCH() string { return vBsvGitCommitTagOrBranch }
func BSV_GIT_COMMIT_HASH() string          { return vBsvGitCommitHash }
func BSV_GIT_COMMIT_DATETIME() string      { return vBsvGitCommitDatetime }

func BDK_VERSION_MAJOR() int     { return int(vBdkVersionMajor) }
func BDK_VERSION_MINOR() int     { return int(vBdkVersionMinor) }
func BDK_VERSION_PATCH() int     { return int(vBdkVersionPatch) }
func BDK_VERSION_STRING() string { return vBdkVersionString }

func SOURCE_GIT_COMMIT_TAG_OR_BRANCH() string { return vSourceGitCommitTagOrBranch }
func SOURCE_GIT_COMMIT_HASH() string          { return vSourceGitCommitHash }
func SOURCE_GIT_COMMIT_DATETIME() string      { return vSourceGitCommitDatetime }
func BDK_BUILD_DATETIME_UTC() string          { return vBdkBuildDatetimeUTC }

func BDK_GOLANG_VERSION_MAJOR() int     { return int(vBdkGolangVersionMajor) }
func BDK_GOLANG_VERSION_MINOR() int     { return int(vBdkGolangVersionMinor) }
func BDK_GOLANG_VERSION_PATCH() int     { return int(vBdkGolangVersionPatch) }
func BDK_GOLANG_VERSION_STRING() string { return vBdkGolangVersionString }
