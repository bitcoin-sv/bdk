package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"context"
	"errors"
	"sync/atomic"
	"unsafe"
)

// ErrCancelled is matched, through errors.Is, by every result of a call that was
// cancelled before it reached a verdict.
var ErrCancelled = errors.New("validation cancelled before a verdict")

// CancelledError is returned by the *Context functions when ctx was cancelled before
// BDK reached a verdict. It is not a verdict about the transaction and not a fault of
// this node: the transaction is neither valid nor invalid as far as this call knows.
//
// errors.Is(err, ErrCancelled) is true. Unwrap returns context.Cause(ctx), so
// errors.Is(err, context.Canceled) or errors.Is(err, context.DeadlineExceeded) tells
// the two apart, as does any cause given to context.WithCancelCause.
type CancelledError struct {
	cause error
}

func (e *CancelledError) Error() string {
	if e.cause == nil {
		return ErrCancelled.Error()
	}
	return ErrCancelled.Error() + ": " + e.cause.Error()
}

// Is reports whether target is ErrCancelled.
func (e *CancelledError) Is(target error) bool { return target == ErrCancelled }

// Unwrap returns the cause of the cancellation, context.Cause(ctx).
func (e *CancelledError) Unwrap() error { return e.cause }

func newCancelledError(ctx context.Context) error {
	if ctx == nil {
		return &CancelledError{}
	}
	return &CancelledError{cause: context.Cause(ctx)}
}

// withCancelFlag runs call with a cancel flag that ctx's cancellation sets.
//
// The flag is an int32 on the Go heap. C reads it only during the call, as cgo
// allows for a pointer to Go memory holding no Go pointers. context.AfterFunc stores
// to it from its own goroutine, started only if ctx is done (for a Context the context
// package did not build, AfterFunc also watches it with a goroutine until stop());
// stop() unregisters the callback once the call has returned. A store that races with the return lands in
// memory BDK no longer reads, and the closure keeps the flag alive until then.
func withCancelFlag[R any](ctx context.Context, call func(flag *C.int32_t) R) R {
	flag := new(int32)
	stop := context.AfterFunc(ctx, func() { atomic.StoreInt32(flag, 1) })
	defer stop()
	return call((*C.int32_t)(unsafe.Pointer(flag)))
}
