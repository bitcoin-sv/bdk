package bdkcgo

/*
#cgo CFLAGS: -I./..
#cgo LDFLAGS: -L./.. -lbdkcgo/lib/libGoBDK_linux_arm64.a -lstdc++ -lm
#include <bdkcgo/include/gobdk.h>
*/
import "C"

// BDKCGOInfo hold information of the CGO
const BDKCGOInfo = "CGO BDK Linux ARM64"
