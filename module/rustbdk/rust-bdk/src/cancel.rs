use std::sync::atomic::{AtomicI32, Ordering};

/// Cancels a running `*_cancellable` validation call.
///
/// Pass `&CancelFlag` to the call and share the flag (for example in an
/// `Arc`) with whichever thread may cancel it. [`cancel`](Self::cancel) is safe
/// from any thread at any time. At its next check the call returns
/// [`TxError::Cancelled`](crate::TxError::Cancelled) if necessary verification
/// is unfinished. A completed job keeps its verdict even if a later check
/// observes cancellation. A step already started (deserialising, one signature
/// hash, one hash or arithmetic opcode) finishes first. Cancelling after the
/// call's last check leaves it to return its verdict.
///
/// A flag stays cancelled; use a new one per call that may need cancelling.
#[derive(Debug, Default)]
pub struct CancelFlag(AtomicI32);

impl CancelFlag {
    /// A flag that is not cancelled.
    pub const fn new() -> Self {
        Self(AtomicI32::new(0))
    }

    /// Asks every call using this flag to stop.
    pub fn cancel(&self) {
        self.0.store(1, Ordering::Relaxed);
    }

    /// Whether [`cancel`](Self::cancel) has been called.
    pub fn is_cancelled(&self) -> bool {
        self.0.load(Ordering::Relaxed) != 0
    }

    /// The caller-owned `int32_t` the C entry points read. `AtomicI32` has the
    /// layout of `i32`, and the borrow keeps it alive for the call.
    pub(crate) fn as_c_ptr(&self) -> *const i32 {
        self.0.as_ptr().cast_const()
    }
}
