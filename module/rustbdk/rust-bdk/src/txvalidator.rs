use std::os::raw::{c_char, c_int};
use std::ptr;

use crate::error::{take_owned_c_string, translate, TxError};
use crate::validatebatch::ValidateBatch;

/// Protocol era selector for policy getters.
///
/// Core rejects the raw bool pair `isGenesisEnabled && isChronicleEnabled`.
/// This enum makes that input unrepresentable while preserving core's three
/// meaningful eras.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ProtocolEra {
    PreGenesis,
    PostGenesis,
    PostChronicle,
}

impl ProtocolEra {
    /// Mapping for core getters that take `(isGenesisEnabled, isChronicleEnabled)`.
    pub fn genesis_chronicle(self) -> (bool, bool) {
        match self {
            Self::PreGenesis => (false, false),
            Self::PostGenesis => (true, false),
            Self::PostChronicle => (false, true),
        }
    }

    /// Mapping for core getters that only ask whether Genesis is active.
    ///
    /// bitcoin-sv's `protocol_era_tests.cpp` asserts that Chronicle activation
    /// also has Genesis active, so `PostChronicle` maps to `true`.
    pub fn is_genesis(self) -> bool {
        match self {
            Self::PreGenesis => false,
            Self::PostGenesis | Self::PostChronicle => true,
        }
    }
}

/// Safe RAII wrapper around `bsv::CTxValidator`.
///
/// Known network names are owned by core's chain-params registry: `main`,
/// `test`, `regtest`, `stn`, `teratestnet`, and `tstn`. Unknown names make
/// [`TxValidator::new`] return `None`.
///
/// This type is intentionally `!Send + !Sync`. Core policy setters forward into
/// process-global state, so the v1 contract is: configure once immediately
/// after construction, then use the validator forever without interleaving
/// further setter calls with validation.
pub struct TxValidator {
    ptr: bdk_sys::TxValidatorHandle,
}

impl TxValidator {
    pub fn new(net_name: &str) -> Option<Self> {
        let net_len = len_to_c_int(net_name.len()).ok()?;
        let ptr = unsafe {
            bdk_sys::bdkffi_txvalidator_create(net_name.as_ptr().cast(), net_len)
        };

        if ptr.is_null() {
            None
        } else {
            Some(Self { ptr })
        }
    }

    pub fn verify_script(
        &self,
        extended_tx: &[u8],
        utxo_heights: &[i32],
        block_height: i32,
        consensus: bool,
    ) -> Result<(), TxError> {
        let (tx_len, utxo_len) = slice_lengths(extended_tx, utxo_heights)?;
        let raw = unsafe {
            bdk_sys::bdkffi_txvalidator_verify_script(
                self.ptr,
                extended_tx.as_ptr().cast(),
                tx_len,
                utxo_heights.as_ptr(),
                utxo_len,
                block_height,
                consensus,
            )
        };
        translate(raw)
    }

    pub fn verify_script_with_custom_flags(
        &self,
        extended_tx: &[u8],
        utxo_heights: &[i32],
        block_height: i32,
        consensus: bool,
        custom_flags: &[u32],
    ) -> Result<(), TxError> {
        let (tx_len, utxo_len) = slice_lengths(extended_tx, utxo_heights)?;
        let flags_len = len_to_c_int(custom_flags.len()).map_err(|_| TxError::Exception)?;
        let raw = unsafe {
            bdk_sys::bdkffi_txvalidator_verify_script_with_custom_flags(
                self.ptr,
                extended_tx.as_ptr().cast(),
                tx_len,
                utxo_heights.as_ptr(),
                utxo_len,
                block_height,
                consensus,
                custom_flags.as_ptr(),
                flags_len,
            )
        };
        translate(raw)
    }

    pub fn validate_transaction(
        &self,
        extended_tx: &[u8],
        utxo_heights: &[i32],
        block_height: i32,
        consensus: bool,
    ) -> Result<(), TxError> {
        let (tx_len, utxo_len) = slice_lengths(extended_tx, utxo_heights)?;
        let raw = unsafe {
            bdk_sys::bdkffi_txvalidator_validate_transaction(
                self.ptr,
                extended_tx.as_ptr().cast(),
                tx_len,
                utxo_heights.as_ptr(),
                utxo_len,
                block_height,
                consensus,
            )
        };
        translate(raw)
    }

    /// Validates every entry currently owned by `batch`.
    ///
    /// If a previous `ValidateBatch::add` left the shim-side batch shorter than
    /// the Rust-owned batch, this returns `Exception` for every Rust-owned
    /// entry. If the shim returns a result length different from the Rust-owned
    /// length, including the documented single `{EXCEPTION, 0}` fallback, this
    /// also treats every entry as `Exception`.
    pub fn validate_batch(&self, batch: &ValidateBatch) -> Vec<Result<(), TxError>> {
        let expected_len = batch.len();
        if batch.has_shim_mismatch() {
            return exception_results(expected_len);
        }

        let mut result_len: c_int = 0;
        let raw_results = unsafe {
            bdk_sys::bdkffi_txvalidator_validate_batch(
                self.ptr,
                batch.as_raw(),
                &mut result_len,
            )
        };

        copy_batch_results(raw_results, result_len, expected_len)
    }

    pub fn get_sig_op_count(
        &self,
        extended_tx: &[u8],
        utxo_heights: &[i32],
        block_height: i32,
        count_p2sh_sig_ops: bool,
        consensus: bool,
    ) -> Result<u64, String> {
        let (tx_len, utxo_len) = slice_lengths_for_string_error(extended_tx, utxo_heights)?;
        let mut err_str = ptr::null_mut();
        let count = unsafe {
            bdk_sys::bdkffi_txvalidator_get_sig_op_count(
                self.ptr,
                extended_tx.as_ptr().cast(),
                tx_len,
                utxo_heights.as_ptr(),
                utxo_len,
                block_height,
                count_p2sh_sig_ops,
                consensus,
                &mut err_str,
            )
        };

        if err_str.is_null() {
            Ok(count)
        } else {
            Err(take_owned_c_string(err_str))
        }
    }

    pub fn calculate_flags(&self, utxo_height: i32, block_height: i32, consensus: bool) -> u32 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_calculate_flags(
                self.ptr,
                utxo_height,
                block_height,
                consensus,
            )
        }
    }

    pub fn set_max_ops_per_script_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_ops_per_script_policy,
            value,
        )
    }

    pub fn set_max_script_num_length_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_script_num_length_policy,
            value,
        )
    }

    pub fn set_max_script_size_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_script_size_policy,
            value,
        )
    }

    pub fn set_max_pub_keys_per_multisig_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_pub_keys_per_multisig_policy,
            value,
        )
    }

    pub fn set_max_stack_memory_usage(
        &self,
        consensus_value: i64,
        policy_value: i64,
    ) -> Result<(), String> {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        self.call_setter_error(unsafe {
            bdk_sys::bdkffi_txvalidator_set_max_stack_memory_usage(
                self.ptr,
                consensus_value,
                policy_value,
            )
        })
    }

    pub fn set_genesis_activation_height(&self, value: i32) -> Result<(), String> {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        self.call_setter_error(unsafe {
            bdk_sys::bdkffi_txvalidator_set_genesis_activation_height(self.ptr, value)
        })
    }

    pub fn set_chronicle_activation_height(&self, value: i32) -> Result<(), String> {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        self.call_setter_error(unsafe {
            bdk_sys::bdkffi_txvalidator_set_chronicle_activation_height(self.ptr, value)
        })
    }

    pub fn set_genesis_graceful_period(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_genesis_graceful_period,
            value,
        )
    }

    pub fn set_chronicle_graceful_period(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_chronicle_graceful_period,
            value,
        )
    }

    pub fn set_max_tx_size_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(bdk_sys::bdkffi_txvalidator_set_max_tx_size_policy, value)
    }

    pub fn set_max_sig_ops_post_genesis_policy(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_sig_ops_post_genesis_policy,
            value,
        )
    }

    pub fn set_max_sig_ops_policy(&self, value: u64) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_max_sig_ops_policy(self.ptr, value) };
    }

    pub fn set_min_consolidation_factor(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_min_consolidation_factor,
            value,
        )
    }

    pub fn set_max_consolidation_input_script_size(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_max_consolidation_input_script_size,
            value,
        )
    }

    pub fn set_min_conf_consolidation_input(&self, value: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_min_conf_consolidation_input,
            value,
        )
    }

    pub fn set_accept_non_std_consolidation_input(&self, value: bool) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe {
            bdk_sys::bdkffi_txvalidator_set_accept_non_std_consolidation_input(self.ptr, value)
        };
    }

    pub fn set_min_mining_tx_fee(&self, satoshis_per_kb: i64) -> Result<(), String> {
        self.call_i64_setter(
            bdk_sys::bdkffi_txvalidator_set_min_mining_tx_fee,
            satoshis_per_kb,
        )
    }

    pub fn set_data_carrier_size(&self, value: u64) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_data_carrier_size(self.ptr, value) };
    }

    pub fn set_data_carrier(&self, value: bool) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_data_carrier(self.ptr, value) };
    }

    pub fn set_accept_non_standard_output(&self, value: bool) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_accept_non_standard_output(self.ptr, value) };
    }

    pub fn set_require_standard(&self, value: bool) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_require_standard(self.ptr, value) };
    }

    pub fn set_permit_bare_multisig(&self, value: bool) {
        // This setter forwards into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_set_permit_bare_multisig(self.ptr, value) };
    }

    pub fn reset_default(&self) {
        // This resets core policy/global state; observe the type-level
        // set-once-use-forever contract before calling it.
        unsafe { bdk_sys::bdkffi_txvalidator_reset_default(self.ptr) };
    }

    pub fn max_ops_per_script(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_ops_per_script(
                self.ptr,
                era.is_genesis(),
                is_consensus,
            )
        }
    }

    pub fn max_script_num_length(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        let (is_genesis_enabled, is_chronicle_enabled) = era.genesis_chronicle();
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_script_num_length(
                self.ptr,
                is_genesis_enabled,
                is_chronicle_enabled,
                is_consensus,
            )
        }
    }

    pub fn max_script_size(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_script_size(
                self.ptr,
                era.is_genesis(),
                is_consensus,
            )
        }
    }

    pub fn max_pub_keys_per_multisig(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_pub_keys_per_multisig(
                self.ptr,
                era.is_genesis(),
                is_consensus,
            )
        }
    }

    pub fn max_stack_memory_usage(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_stack_memory_usage(
                self.ptr,
                era.is_genesis(),
                is_consensus,
            )
        }
    }

    pub fn max_tx_size(&self, era: ProtocolEra, is_consensus: bool) -> u64 {
        let (is_genesis_enabled, is_chronicle_enabled) = era.genesis_chronicle();
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_tx_size(
                self.ptr,
                is_genesis_enabled,
                is_chronicle_enabled,
                is_consensus,
            )
        }
    }

    pub fn data_carrier_size(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_data_carrier_size(self.ptr) }
    }

    pub fn data_carrier(&self) -> bool {
        unsafe { bdk_sys::bdkffi_txvalidator_get_data_carrier(self.ptr) }
    }

    pub fn accept_non_standard_output(&self, era: ProtocolEra) -> bool {
        let (is_genesis_enabled, is_chronicle_enabled) = era.genesis_chronicle();
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_accept_non_standard_output(
                self.ptr,
                is_genesis_enabled,
                is_chronicle_enabled,
            )
        }
    }

    pub fn require_standard(&self) -> bool {
        unsafe { bdk_sys::bdkffi_txvalidator_get_require_standard(self.ptr) }
    }

    pub fn permit_bare_multisig(&self) -> bool {
        unsafe { bdk_sys::bdkffi_txvalidator_get_permit_bare_multisig(self.ptr) }
    }

    pub fn genesis_activation_height(&self) -> i32 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_genesis_activation_height(self.ptr) }
    }

    pub fn chronicle_activation_height(&self) -> i32 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_chronicle_activation_height(self.ptr) }
    }

    pub fn genesis_graceful_period(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_genesis_graceful_period(self.ptr) }
    }

    pub fn chronicle_graceful_period(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_chronicle_graceful_period(self.ptr) }
    }

    pub fn min_consolidation_factor(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_min_consolidation_factor(self.ptr) }
    }

    pub fn max_consolidation_input_script_size(&self) -> u64 {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_max_consolidation_input_script_size(self.ptr)
        }
    }

    pub fn min_conf_consolidation_input(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_min_conf_consolidation_input(self.ptr) }
    }

    pub fn accept_non_std_consolidation_input(&self) -> bool {
        unsafe {
            bdk_sys::bdkffi_txvalidator_get_accept_non_std_consolidation_input(self.ptr)
        }
    }

    pub fn min_mining_tx_fee(&self) -> i64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_min_mining_tx_fee(self.ptr) }
    }

    pub fn max_sig_ops_policy(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_max_sig_ops_policy(self.ptr) }
    }

    pub fn max_sig_ops_post_genesis_policy(&self) -> u64 {
        unsafe { bdk_sys::bdkffi_txvalidator_get_max_sig_ops_post_genesis_policy(self.ptr) }
    }

    fn call_i64_setter(
        &self,
        setter: unsafe extern "C" fn(bdk_sys::TxValidatorHandle, i64) -> *mut c_char,
        value: i64,
    ) -> Result<(), String> {
        // These setters forward into core policy/global state; observe the
        // type-level set-once-use-forever contract before calling them.
        self.call_setter_error(unsafe { setter(self.ptr, value) })
    }

    fn call_setter_error(&self, err: *mut c_char) -> Result<(), String> {
        if err.is_null() {
            Ok(())
        } else {
            Err(take_owned_c_string(err))
        }
    }
}

impl Drop for TxValidator {
    fn drop(&mut self) {
        unsafe { bdk_sys::bdkffi_txvalidator_destroy(self.ptr) };
    }
}

fn slice_lengths(extended_tx: &[u8], utxo_heights: &[i32]) -> Result<(c_int, c_int), TxError> {
    let tx_len = len_to_c_int(extended_tx.len()).map_err(|_| TxError::Exception)?;
    let utxo_len = len_to_c_int(utxo_heights.len()).map_err(|_| TxError::Exception)?;
    Ok((tx_len, utxo_len))
}

fn slice_lengths_for_string_error(
    extended_tx: &[u8],
    utxo_heights: &[i32],
) -> Result<(c_int, c_int), String> {
    let tx_len = len_to_c_int(extended_tx.len())
        .map_err(|_| "extended transaction length exceeds C ABI limit".to_owned())?;
    let utxo_len = len_to_c_int(utxo_heights.len())
        .map_err(|_| "utxo heights length exceeds C ABI limit".to_owned())?;
    Ok((tx_len, utxo_len))
}

fn len_to_c_int(len: usize) -> Result<c_int, ()> {
    if len > c_int::MAX as usize {
        Err(())
    } else {
        Ok(len as c_int)
    }
}

fn copy_batch_results(
    raw_results: *mut bdk_sys::TxError,
    result_len: c_int,
    expected_len: usize,
) -> Vec<Result<(), TxError>> {
    if result_len < 0 || result_len as usize != expected_len {
        if !raw_results.is_null() {
            unsafe { bdk_sys::bdkffi_free(raw_results.cast()) };
        }
        return exception_results(expected_len);
    }

    if expected_len == 0 {
        if !raw_results.is_null() {
            unsafe { bdk_sys::bdkffi_free(raw_results.cast()) };
        }
        return Vec::new();
    }

    if raw_results.is_null() {
        return exception_results(expected_len);
    }

    let raw = unsafe { std::slice::from_raw_parts(raw_results, expected_len) }.to_vec();
    unsafe { bdk_sys::bdkffi_free(raw_results.cast()) };
    raw.into_iter().map(translate).collect()
}

fn exception_results(len: usize) -> Vec<Result<(), TxError>> {
    std::iter::repeat_with(|| Err(TxError::Exception))
        .take(len)
        .collect()
}
