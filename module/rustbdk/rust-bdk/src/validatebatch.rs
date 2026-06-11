use std::marker::PhantomData;
use std::os::raw::c_int;

/// Owning batch of transaction-validation inputs.
///
/// Core stores non-owning spans inside the C++ batch, so this wrapper owns one
/// `Vec<u8>` and one `Vec<i32>` per entry. The soundness invariants are:
///
/// - [`ValidateBatch::add`] checks C ABI lengths, copies both input slices into
///   owned inner vectors, pushes those vectors first, then hands pointers from
///   `last().as_ptr()` to the shim.
/// - The inner vector heap buffers stay stable even if the outer `Vec<Vec<_>>`
///   reallocates. This type must never use slices of a single flattened buffer
///   that can move on reallocation.
/// - [`ValidateBatch::clear`] clears the shim batch and the owned buffers
///   together, so C++ never intentionally keeps spans to released Rust storage.
/// - [`crate::TxValidator::validate_batch`] borrows the batch for the whole FFI
///   call, keeping all backing buffers alive while core reads them.
/// - If the shim swallows an internal exception during `add` and its size no
///   longer matches the Rust-owned entry count, the mismatch is recorded and
///   remains visible via [`ValidateBatch::has_shim_mismatch`] until `clear()`.
///
/// This type is intentionally `!Send + !Sync`; the raw C handle has no thread
/// safety contract in v1.
pub struct ValidateBatch {
    ptr: bdk_sys::ValidateBatchHandle,
    txs: Vec<Vec<u8>>,
    utxos: Vec<Vec<i32>>,
    shim_mismatch: bool,
}

impl ValidateBatch {
    pub fn new() -> Self {
        Self::with_capacity(0)
    }

    /// Creates an empty batch with room for at least `capacity` entries.
    ///
    /// # Panics
    ///
    /// Panics if `capacity` exceeds `c_int::MAX`, or if the shim cannot create
    /// the underlying C++ batch.
    pub fn with_capacity(capacity: usize) -> Self {
        let ffi_capacity = len_to_c_int(capacity).expect("batch capacity exceeds C ABI limit");
        let ptr = unsafe { bdk_sys::bdkffi_validatebatch_create() };
        if ptr.is_null() {
            panic!("failed to create BDK validate batch");
        }

        if ffi_capacity > 0 {
            unsafe { bdk_sys::bdkffi_validatebatch_reserve(ptr, ffi_capacity) };
        }

        Self {
            ptr,
            txs: Vec::with_capacity(capacity),
            utxos: Vec::with_capacity(capacity),
            shim_mismatch: false,
        }
    }

    /// Adds one entry by copying inputs into owned storage before calling the
    /// shim with stable inner-vector pointers.
    ///
    /// If the shim catches an internal exception while adding, it cannot return
    /// the error through this `()` API. This wrapper immediately compares the
    /// shim size with the Rust-owned length and records any mismatch; while that
    /// flag is set, `TxValidator::validate_batch` returns `Exception` for every
    /// Rust-owned entry instead of pretending the batch is complete.
    ///
    /// # Panics
    ///
    /// Panics if either input slice length, or the total batch entry count,
    /// exceeds `c_int::MAX`.
    pub fn add(
        &mut self,
        extended_tx: &[u8],
        utxo_heights: &[i32],
        block_height: i32,
        consensus: bool,
    ) {
        let tx_len = len_to_c_int(extended_tx.len())
            .expect("extended transaction length exceeds C ABI limit");
        let utxo_len =
            len_to_c_int(utxo_heights.len()).expect("utxo heights length exceeds C ABI limit");
        if self.txs.len() == c_int::MAX as usize {
            panic!("batch length exceeds C ABI limit");
        }

        let tx = extended_tx.to_vec();
        let utxo = utxo_heights.to_vec();
        self.txs.reserve(1);
        self.utxos.reserve(1);
        self.txs.push(tx);
        self.utxos.push(utxo);

        let tx_ptr = self.txs.last().expect("just pushed tx").as_ptr();
        let utxo_ptr = self.utxos.last().expect("just pushed utxos").as_ptr();
        unsafe {
            bdk_sys::bdkffi_validatebatch_add(
                self.ptr,
                tx_ptr.cast(),
                tx_len,
                utxo_ptr,
                utxo_len,
                block_height,
                consensus,
            )
        };

        let shim_len = unsafe { bdk_sys::bdkffi_validatebatch_size(self.ptr) };
        if shim_len < 0 || shim_len as usize != self.txs.len() {
            self.shim_mismatch = true;
        }
    }

    /// Clears both the shim batch and the Rust-owned backing buffers.
    pub fn clear(&mut self) {
        unsafe { bdk_sys::bdkffi_validatebatch_clear(self.ptr) };
        self.txs.clear();
        self.utxos.clear();
        self.shim_mismatch = false;
    }

    pub fn len(&self) -> usize {
        debug_assert_eq!(self.txs.len(), self.utxos.len());
        self.txs.len()
    }

    pub fn is_empty(&self) -> bool {
        self.txs.is_empty()
    }

    /// Returns true if a shim-side `add` failed after Rust had already copied
    /// and recorded the entry.
    ///
    /// The batch remains memory-safe, but its C++ side is incomplete. Call
    /// [`ValidateBatch::clear`] to restore a reusable empty batch.
    pub fn has_shim_mismatch(&self) -> bool {
        self.shim_mismatch
    }

    /// Ensures room for at least `capacity` total entries in both Rust storage
    /// and the shim batch.
    ///
    /// # Panics
    ///
    /// Panics if `capacity` exceeds `c_int::MAX`.
    pub fn reserve(&mut self, capacity: usize) {
        let ffi_capacity = len_to_c_int(capacity).expect("batch capacity exceeds C ABI limit");
        reserve_total(&mut self.txs, capacity);
        reserve_total(&mut self.utxos, capacity);
        unsafe { bdk_sys::bdkffi_validatebatch_reserve(self.ptr, ffi_capacity) };
    }

    pub(crate) fn as_raw(&self) -> bdk_sys::ValidateBatchHandle {
        self.ptr
    }
}

impl Default for ValidateBatch {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for ValidateBatch {
    fn drop(&mut self) {
        unsafe { bdk_sys::bdkffi_validatebatch_destroy(self.ptr) };
    }
}

/// Placeholder for the optional zero-copy borrowing batch described in the
/// design. It remains intentionally non-constructible until Stage 8 benchmarks
/// show that the owning batch copy cost matters.
pub struct ValidateBatchRef<'a> {
    _marker: PhantomData<&'a [u8]>,
}

fn reserve_total<T>(vec: &mut Vec<T>, capacity: usize) {
    if capacity > vec.capacity() {
        vec.reserve(capacity - vec.len());
    }
}

fn len_to_c_int(len: usize) -> Result<c_int, ()> {
    if len > c_int::MAX as usize {
        Err(())
    } else {
        Ok(len as c_int)
    }
}
