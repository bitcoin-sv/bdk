use std::os::raw::c_int;

/// Safe wrappers for Criterion microbenchmarks.
///
/// These functions are public so benchmark targets can stay on the safe crate
/// boundary. They are not part of the stable application-facing API.
pub fn ffi_noop() {
    unsafe { bdk_sys::bdkffi_bench_noop() };
}

pub fn sum_bytes(bytes: &[u8]) -> u64 {
    let len = len_to_c_int(bytes.len()).expect("byte slice length exceeds C ABI limit");
    unsafe { bdk_sys::bdkffi_bench_sum_bytes(bytes.as_ptr(), len) }
}

fn len_to_c_int(len: usize) -> Result<c_int, ()> {
    if len > c_int::MAX as usize {
        Err(())
    } else {
        Ok(len as c_int)
    }
}
