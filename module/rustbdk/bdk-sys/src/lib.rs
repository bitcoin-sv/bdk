#![allow(non_camel_case_types, non_snake_case)]

use std::os::raw::{c_char, c_int, c_void};

pub type TxValidatorHandle = *mut c_void;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TxError {
    pub domain: i32,
    pub code: i32,
}

pub const TX_ERR_DOMAIN_OK: i32 = 0;
pub const TX_ERR_DOMAIN_SCRIPT: i32 = 1;
pub const TX_ERR_DOMAIN_DOS: i32 = 2;
pub const TX_ERR_DOMAIN_EXCEPTION: i32 = 3;

unsafe extern "C" {
    pub fn bdkffi_txvalidator_create(
        network_name: *const c_char,
        network_name_len: c_int,
    ) -> TxValidatorHandle;
    pub fn bdkffi_txvalidator_destroy(validator: TxValidatorHandle);

    pub fn bdkffi_txvalidator_verify_script(
        validator: TxValidatorHandle,
        extended_tx: *const c_char,
        extended_tx_len: c_int,
        utxo_heights: *const i32,
        utxo_heights_len: c_int,
        block_height: i32,
        consensus: bool,
    ) -> TxError;

    pub fn bdkffi_cpp_script_err_error_count() -> c_int;

    /// Frees every caller-owned buffer returned by the bdkffi C ABI.
    pub fn bdkffi_free(p: *mut c_void);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn smoke_create_error_count_destroy() {
        const EXPECTED_SCRIPT_ERR_ERROR_COUNT: c_int = 47;

        let network = b"main";
        let validator = unsafe {
            bdkffi_txvalidator_create(
                network.as_ptr().cast::<c_char>(),
                network.len() as c_int,
            )
        };
        assert!(!validator.is_null());

        let script_error_count = unsafe { bdkffi_cpp_script_err_error_count() };
        unsafe { bdkffi_txvalidator_destroy(validator) };

        assert_eq!(script_error_count, EXPECTED_SCRIPT_ERR_ERROR_COUNT);
    }
}
