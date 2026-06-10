#[test]
fn link_smoke() {
    // Exact script-error count parity belongs in the Stage 4 safe-crate test.
    // This integration test only proves the raw symbol links and returns data.
    let script_error_count = unsafe { bdk_sys::bdkffi_cpp_script_err_error_count() };
    assert!(script_error_count > 0);

    let batch = unsafe { bdk_sys::bdkffi_validatebatch_create() };
    assert!(!batch.is_null());
    unsafe { bdk_sys::bdkffi_validatebatch_destroy(batch) };
}
