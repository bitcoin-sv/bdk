#![allow(non_camel_case_types, non_snake_case)]

use std::os::raw::{c_char, c_int, c_void};

// bdkffi/txerror.h
// Keep C-spelled names beside the shorter Stage-2 Rust names intentionally:
// the bdkffi_* surface mirrors the headers, while the shorter aliases preserve
// the existing Rust-facing raw API for later safe-wrapper stages.
//
// The C enum alias is only for the constants below. The ABI-carrying
// bdkffi_TxError.domain field is int32_t in the header and is mapped to i32
// independently of the enum alias width.
pub type bdkffi_tx_error_domain = c_int;

pub const BDKFFI_TX_ERR_DOMAIN_OK: bdkffi_tx_error_domain = 0;
pub const BDKFFI_TX_ERR_DOMAIN_SCRIPT: bdkffi_tx_error_domain = 1;
pub const BDKFFI_TX_ERR_DOMAIN_DOS: bdkffi_tx_error_domain = 2;
pub const BDKFFI_TX_ERR_DOMAIN_EXCEPTION: bdkffi_tx_error_domain = 3;

pub const TX_ERR_DOMAIN_OK: i32 = BDKFFI_TX_ERR_DOMAIN_OK;
pub const TX_ERR_DOMAIN_SCRIPT: i32 = BDKFFI_TX_ERR_DOMAIN_SCRIPT;
pub const TX_ERR_DOMAIN_DOS: i32 = BDKFFI_TX_ERR_DOMAIN_DOS;
pub const TX_ERR_DOMAIN_EXCEPTION: i32 = BDKFFI_TX_ERR_DOMAIN_EXCEPTION;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TxError {
    /// Mirrors bdkffi_TxError.domain: int32_t in the C ABI.
    pub domain: i32,
    pub code: i32,
}

pub type bdkffi_TxError = TxError;

// bdkffi/txvalidator.h
// bdkffi_*_t mirrors the C typedef; *Handle preserves the Stage-2 Rust alias.
pub type bdkffi_txvalidator_t = *mut c_void;
pub type TxValidatorHandle = bdkffi_txvalidator_t;

// bdkffi/validatebatch.h
// bdkffi_*_t mirrors the C typedef; *Handle preserves the Stage-2 Rust alias.
pub type bdkffi_validatebatch_t = *mut c_void;
pub type ValidateBatchHandle = bdkffi_validatebatch_t;

// bdkffi/alloc.h
unsafe extern "C" {
    /// Frees every caller-owned buffer returned by the bdkffi C ABI.
    pub fn bdkffi_free(p: *mut c_void);
}

// bdkffi/bench.h
#[cfg(feature = "bench")]
unsafe extern "C" {
    pub fn bdkffi_bench_noop();
    pub fn bdkffi_bench_sum_bytes(data: *const u8, data_len: c_int) -> u64;
}

// bdkffi/asm.h
unsafe extern "C" {
    pub fn bdkffi_from_asm(
        asm_ptr: *const c_char,
        asm_len: c_int,
        script_len: *mut c_int,
    ) -> *mut c_char;

    pub fn bdkffi_to_asm(script_ptr: *const c_char, script_len: c_int) -> *mut c_char;
}

// bdkffi/error_strings.h
unsafe extern "C" {
    /*
     * Return malloc-owned null-terminated strings. Free with bdkffi_free.
     */
    pub fn bdkffi_script_error_string(code: c_int) -> *mut c_char;
    pub fn bdkffi_dos_error_string(code: c_int) -> *mut c_char;
    pub fn bdkffi_cpp_script_err_error_count() -> c_int;
}

// bdkffi/version.h
unsafe extern "C" {
    pub fn bdkffi_bsv_client_version_major() -> c_int;
    pub fn bdkffi_bsv_client_version_minor() -> c_int;
    pub fn bdkffi_bsv_client_version_revision() -> c_int;
    pub fn bdkffi_bsv_version_string() -> *const c_char;

    pub fn bdkffi_bsv_git_commit_tag_or_branch() -> *const c_char;
    pub fn bdkffi_bsv_git_commit_hash() -> *const c_char;
    pub fn bdkffi_bsv_git_commit_datetime() -> *const c_char;

    pub fn bdkffi_bdk_version_major() -> c_int;
    pub fn bdkffi_bdk_version_minor() -> c_int;
    pub fn bdkffi_bdk_version_patch() -> c_int;
    pub fn bdkffi_bdk_version_string() -> *const c_char;

    pub fn bdkffi_source_git_commit_tag_or_branch() -> *const c_char;
    pub fn bdkffi_source_git_commit_hash() -> *const c_char;
    pub fn bdkffi_source_git_commit_datetime() -> *const c_char;
    pub fn bdkffi_bdk_build_datetime_utc() -> *const c_char;

    pub fn bdkffi_bdk_rust_version_major() -> c_int;
    pub fn bdkffi_bdk_rust_version_minor() -> c_int;
    pub fn bdkffi_bdk_rust_version_patch() -> c_int;
    pub fn bdkffi_bdk_rust_version_string() -> *const c_char;
}

// bdkffi/txvalidator.h
unsafe extern "C" {
    pub fn bdkffi_txvalidator_create(
        network_name: *const c_char,
        network_name_len: c_int,
    ) -> TxValidatorHandle;

    pub fn bdkffi_txvalidator_destroy(validator: TxValidatorHandle);

    /*
     * Fallible setters return NULL on success, otherwise a malloc-owned
     * null-terminated error string. Free it with bdkffi_free.
     */
    pub fn bdkffi_txvalidator_set_max_ops_per_script_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_script_num_length_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_script_size_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_pub_keys_per_multisig_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_stack_memory_usage(
        validator: TxValidatorHandle,
        consensus_value: i64,
        policy_value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_genesis_activation_height(
        validator: TxValidatorHandle,
        value: i32,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_chronicle_activation_height(
        validator: TxValidatorHandle,
        value: i32,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_genesis_graceful_period(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_chronicle_graceful_period(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_tx_size_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_sig_ops_post_genesis_policy(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_sig_ops_policy(
        validator: TxValidatorHandle,
        value: u64,
    );

    pub fn bdkffi_txvalidator_set_min_consolidation_factor(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_max_consolidation_input_script_size(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_min_conf_consolidation_input(
        validator: TxValidatorHandle,
        value: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_accept_non_std_consolidation_input(
        validator: TxValidatorHandle,
        value: bool,
    );

    pub fn bdkffi_txvalidator_set_min_mining_tx_fee(
        validator: TxValidatorHandle,
        satoshis_per_kb: i64,
    ) -> *mut c_char;

    pub fn bdkffi_txvalidator_set_data_carrier_size(validator: TxValidatorHandle, value: u64);
    pub fn bdkffi_txvalidator_set_data_carrier(validator: TxValidatorHandle, value: bool);
    pub fn bdkffi_txvalidator_set_accept_non_standard_output(
        validator: TxValidatorHandle,
        value: bool,
    );
    pub fn bdkffi_txvalidator_set_require_standard(validator: TxValidatorHandle, value: bool);
    pub fn bdkffi_txvalidator_set_permit_bare_multisig(validator: TxValidatorHandle, value: bool);
    pub fn bdkffi_txvalidator_reset_default(validator: TxValidatorHandle);

    pub fn bdkffi_txvalidator_get_max_ops_per_script(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_max_script_num_length(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_chronicle_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_max_script_size(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_max_pub_keys_per_multisig(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_max_stack_memory_usage(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_max_tx_size(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_chronicle_enabled: bool,
        is_consensus: bool,
    ) -> u64;

    pub fn bdkffi_txvalidator_get_data_carrier_size(validator: TxValidatorHandle) -> u64;
    pub fn bdkffi_txvalidator_get_data_carrier(validator: TxValidatorHandle) -> bool;

    pub fn bdkffi_txvalidator_get_accept_non_standard_output(
        validator: TxValidatorHandle,
        is_genesis_enabled: bool,
        is_chronicle_enabled: bool,
    ) -> bool;

    pub fn bdkffi_txvalidator_get_require_standard(validator: TxValidatorHandle) -> bool;
    pub fn bdkffi_txvalidator_get_permit_bare_multisig(validator: TxValidatorHandle) -> bool;
    pub fn bdkffi_txvalidator_get_genesis_activation_height(
        validator: TxValidatorHandle,
    ) -> i32;
    pub fn bdkffi_txvalidator_get_chronicle_activation_height(
        validator: TxValidatorHandle,
    ) -> i32;
    pub fn bdkffi_txvalidator_get_genesis_graceful_period(validator: TxValidatorHandle) -> u64;
    pub fn bdkffi_txvalidator_get_chronicle_graceful_period(validator: TxValidatorHandle) -> u64;
    pub fn bdkffi_txvalidator_get_min_consolidation_factor(validator: TxValidatorHandle) -> u64;
    pub fn bdkffi_txvalidator_get_max_consolidation_input_script_size(
        validator: TxValidatorHandle,
    ) -> u64;
    pub fn bdkffi_txvalidator_get_min_conf_consolidation_input(
        validator: TxValidatorHandle,
    ) -> u64;
    pub fn bdkffi_txvalidator_get_accept_non_std_consolidation_input(
        validator: TxValidatorHandle,
    ) -> bool;
    pub fn bdkffi_txvalidator_get_min_mining_tx_fee(validator: TxValidatorHandle) -> i64;
    pub fn bdkffi_txvalidator_get_max_sig_ops_policy(validator: TxValidatorHandle) -> u64;
    pub fn bdkffi_txvalidator_get_max_sig_ops_post_genesis_policy(
        validator: TxValidatorHandle,
    ) -> u64;

    /*
     * On success, err_str is set to NULL and the return value is the sigop count.
     * On failure, the return value is 0 and err_str receives a malloc-owned,
     * null-terminated error string when err_str is non-NULL. Free it with
     * bdkffi_free.
     */
    pub fn bdkffi_txvalidator_get_sig_op_count(
        validator: TxValidatorHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        count_p2sh_sig_ops: bool,
        consensus: bool,
        err_str: *mut *mut c_char,
    ) -> u64;

    pub fn bdkffi_txvalidator_calculate_flags(
        validator: TxValidatorHandle,
        utxo_height: i32,
        block_height: i32,
        consensus: bool,
    ) -> u32;

    pub fn bdkffi_txvalidator_verify_script(
        validator: TxValidatorHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        consensus: bool,
    ) -> TxError;

    pub fn bdkffi_txvalidator_verify_script_with_custom_flags(
        validator: TxValidatorHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        consensus: bool,
        custom_flags: *const u32,
        custom_flags_len: c_int,
    ) -> TxError;

    pub fn bdkffi_txvalidator_validate_transaction(
        validator: TxValidatorHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        consensus: bool,
    ) -> TxError;

    /*
     * Returns a malloc-owned array of bdkffi_TxError values and writes its length
     * to result_size. Free the returned pointer with bdkffi_free.
     *
     * Normal success and per-transaction validation failures preserve positional
     * mapping with the batch size. If the shim itself catches an internal exception
     * before core can return the vector, result_size is 1 and the single element is
     * {BDKFFI_TX_ERR_DOMAIN_EXCEPTION, 0}; safe wrappers should treat a length
     * mismatch as all entries failing with EXCEPTION.
     */
    pub fn bdkffi_txvalidator_validate_batch(
        validator: TxValidatorHandle,
        batch: ValidateBatchHandle,
        result_size: *mut c_int,
    ) -> *mut TxError;
}

// bdkffi/validatebatch.h
unsafe extern "C" {
    pub fn bdkffi_validatebatch_create() -> ValidateBatchHandle;
    pub fn bdkffi_validatebatch_destroy(batch: ValidateBatchHandle);

    /*
     * Adds a validation argument using non-owning spans over caller-provided memory.
     * The caller must keep extended_tx and utxo_heights alive until the batch is
     * cleared or destroyed. bdkffi_txvalidator_validate_batch borrows the batch
     * and does not clear it, so the batch may be reused.
     */
    pub fn bdkffi_validatebatch_add(
        batch: ValidateBatchHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        consensus: bool,
    );

    pub fn bdkffi_validatebatch_clear(batch: ValidateBatchHandle);
    pub fn bdkffi_validatebatch_size(batch: ValidateBatchHandle) -> c_int;
    pub fn bdkffi_validatebatch_empty(batch: ValidateBatchHandle) -> bool;
    pub fn bdkffi_validatebatch_reserve(batch: ValidateBatchHandle, capacity: c_int);
}
