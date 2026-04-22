package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

// DoSErrorCode mirrors DoSError_t from core/doserror.hpp.
type DoSErrorCode int

const (
	DOS_ERR_OK                       DoSErrorCode = 0
	DOS_ERR_NULL_PREVOUT             DoSErrorCode = 1
	DOS_ERR_P2SH_OUTPUT_POST_GENESIS DoSErrorCode = 2
	DOS_ERR_SIGOPS_CONSENSUS         DoSErrorCode = 3
	DOS_ERR_SIGOPS_POLICY            DoSErrorCode = 4
	DOS_ERR_NOT_FREE_CONSOLIDATION   DoSErrorCode = 5
	DOS_ERR_NOT_STANDARD             DoSErrorCode = 6
	DOS_ERR_COUNT                    DoSErrorCode = 7
)

// DoSError is the Go error type for transaction-level validation failures.
type DoSError interface {
	error
	Code() DoSErrorCode
}

type doSErrorImpl struct{ code DoSErrorCode }

// NewDoSError creates a DoSError for the given code.
func NewDoSError(code DoSErrorCode) DoSError { return &doSErrorImpl{code: code} }

func (e *doSErrorImpl) Code() DoSErrorCode { return e.code }
func (e *doSErrorImpl) Error() string {
	return C.GoString(C.dos_error_string(C.int(e.code)))
}
