package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

// ABIErrorCode mirrors AbiError_t from core/abierror.h.
type ABIErrorCode int

const (
	ABI_ERR_OK                       ABIErrorCode = 0
	ABI_ERR_LENGTH_NEGATIVE          ABIErrorCode = 1
	ABI_ERR_LENGTH_NOT_REPRESENTABLE ABIErrorCode = 2
	ABI_ERR_NULL_BUFFER              ABIErrorCode = 3
	ABI_ERR_LENGTH_OVERFLOW          ABIErrorCode = 4
	ABI_ERR_COUNT                    ABIErrorCode = 5
)

// ABIError is the Go error type for a call this binding could not express across the
// C ABI. It says nothing about the transaction: it reports a local defect — a length
// that cannot be represented, or a length inconsistent with its pointer.
//
// A consumer must classify an ABIError as a node fault, never as a transaction
// verdict. Treating it as an invalid transaction would let a local binding defect
// reject a consensus-valid transaction.
type ABIError interface {
	error
	Code() ABIErrorCode
}

type abiErrorImpl struct{ code ABIErrorCode }

// NewABIError creates an ABIError for the given code.
func NewABIError(code ABIErrorCode) ABIError { return &abiErrorImpl{code: code} }

func (e *abiErrorImpl) Code() ABIErrorCode { return e.code }
func (e *abiErrorImpl) Error() string {
	return C.GoString(C.abi_error_string(C.int(e.code)))
}
