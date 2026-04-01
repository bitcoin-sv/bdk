//go:build !cgo || purego

package bdkpurego

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync"
	"unsafe"

	"github.com/ebitengine/purego"
)

var (
	initOnce sync.Once
	initErr  error

	libHandle uintptr

	// libc functions
	cFree   func(ptr uintptr)
	cMalloc func(size uintptr) uintptr
)

// Init initializes the shared library. Safe to call multiple times.
func Init() error {
	initOnce.Do(func() {
		initErr = doInit()
	})
	return initErr
}

// Handle returns the loaded library handle. Must call Init first.
func Handle() uintptr {
	return libHandle
}

func doInit() error {
	path, err := findLibrary()
	if err != nil {
		return fmt.Errorf("bdkpurego: %w", err)
	}

	libHandle, err = purego.Dlopen(path, purego.RTLD_NOW|purego.RTLD_GLOBAL)
	if err != nil {
		return fmt.Errorf("bdkpurego: dlopen %s: %w", path, err)
	}

	// Load libc for memory management
	libcHandle, err := openLibc()
	if err != nil {
		return fmt.Errorf("bdkpurego: open libc: %w", err)
	}

	purego.RegisterLibFunc(&cFree, libcHandle, "free")
	purego.RegisterLibFunc(&cMalloc, libcHandle, "malloc")

	return nil
}

func openLibc() (uintptr, error) {
	switch runtime.GOOS {
	case "darwin":
		return purego.Dlopen("libSystem.B.dylib", purego.RTLD_NOW|purego.RTLD_GLOBAL)
	case "linux":
		// Try common libc paths
		for _, name := range []string{"libc.so.6", "libc.so"} {
			h, err := purego.Dlopen(name, purego.RTLD_NOW|purego.RTLD_GLOBAL)
			if err == nil {
				return h, nil
			}
		}
		return 0, fmt.Errorf("could not open libc")
	default:
		return 0, fmt.Errorf("unsupported OS: %s", runtime.GOOS)
	}
}

func findLibrary() (string, error) {
	// 1. Check BDK_LIB_PATH environment variable
	if p := os.Getenv("BDK_LIB_PATH"); p != "" {
		if _, err := os.Stat(p); err == nil {
			return p, nil
		}
	}

	// 2. Check alongside the bdkcgo directory (same location as static .a files)
	_, thisFile, _, _ := runtime.Caller(0)
	bdkcgoDir := filepath.Join(filepath.Dir(thisFile), "..", "bdkcgo")
	candidate := filepath.Join(bdkcgoDir, LibraryFileName)
	if _, err := os.Stat(candidate); err == nil {
		return candidate, nil
	}

	// 3. Try the library name directly (relies on system library search path)
	return LibraryFileName, nil
}

// Free releases memory allocated by the C library.
func Free(ptr uintptr) {
	if ptr != 0 {
		cFree(ptr)
	}
}

// Malloc allocates memory via the C library.
func Malloc(size uintptr) uintptr {
	return cMalloc(size)
}

// ptrToBytePtr converts a uintptr to *byte for use with unsafe.Slice.
// This is safe for pointers obtained from C allocators (malloc/Dlsym).
// go vet flags this as "possible misuse of unsafe.Pointer" but this is the
// standard pattern for FFI code where pointers originate from C.
func ptrToBytePtr(ptr uintptr) *byte {
	return (*byte)(unsafe.Pointer(ptr)) //nolint:govet
}

// GoString reads a null-terminated C string at ptr into a Go string.
func GoString(ptr uintptr) string {
	if ptr == 0 {
		return ""
	}
	p := ptrToBytePtr(ptr)
	// Find the null terminator
	var length int
	for {
		b := *(*byte)(unsafe.Add(unsafe.Pointer(p), length))
		if b == 0 {
			break
		}
		length++
	}
	if length == 0 {
		return ""
	}
	return string(unsafe.Slice(p, length))
}

// GoBytes copies length bytes from ptr into a new Go byte slice.
func GoBytes(ptr uintptr, length int) []byte {
	if ptr == 0 || length == 0 {
		return nil
	}
	buf := make([]byte, length)
	copy(buf, unsafe.Slice(ptrToBytePtr(ptr), length))
	return buf
}

// CString allocates a null-terminated C string from a Go string.
// The caller must Free the returned pointer.
func CString(s string) (uintptr, int) {
	n := len(s)
	ptr := Malloc(uintptr(n + 1))
	if ptr == 0 {
		return 0, 0
	}
	dst := unsafe.Slice(ptrToBytePtr(ptr), n+1)
	copy(dst, s)
	dst[n] = 0
	return ptr, n
}

// SliceDataPtr returns the pointer to the first element of a byte slice,
// or 0 if the slice is empty.
func SliceDataPtr[T any](s []T) uintptr {
	if len(s) == 0 {
		return 0
	}
	return uintptr(unsafe.Pointer(&s[0]))
}
