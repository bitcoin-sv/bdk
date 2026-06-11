use std::ffi::CStr;
use std::os::raw::{c_char, c_int};

/// Converts assembler text to script bytes.
///
/// The C shim returns a malloc-owned buffer. This wrapper copies it into an
/// owned `Vec<u8>` and releases the C buffer with `bdkffi_free`.
///
/// # Panics
///
/// Panics if `asm.len()` exceeds `c_int::MAX`.
pub fn from_asm(asm: &str) -> Vec<u8> {
    let asm_len = len_to_c_int(asm.len()).expect("asm length exceeds C ABI limit");
    let mut script_len: c_int = 0;
    let ptr = unsafe {
        bdk_sys::bdkffi_from_asm(asm.as_ptr().cast::<c_char>(), asm_len, &mut script_len)
    };

    if ptr.is_null() {
        return Vec::new();
    }

    if script_len <= 0 {
        unsafe { bdk_sys::bdkffi_free(ptr.cast()) };
        return Vec::new();
    }

    let script = unsafe {
        std::slice::from_raw_parts(ptr.cast::<u8>(), script_len as usize).to_vec()
    };
    unsafe { bdk_sys::bdkffi_free(ptr.cast()) };
    script
}

/// Converts script bytes to assembler text.
///
/// The C shim returns a malloc-owned C string. This wrapper copies it into an
/// owned `String` and releases the C string with `bdkffi_free`.
///
/// # Panics
///
/// Panics if `script.len()` exceeds `c_int::MAX`.
pub fn to_asm(script: &[u8]) -> String {
    let script_len = len_to_c_int(script.len()).expect("script length exceeds C ABI limit");
    let ptr = unsafe { bdk_sys::bdkffi_to_asm(script.as_ptr().cast::<c_char>(), script_len) };

    if ptr.is_null() {
        return String::new();
    }

    let asm = unsafe { CStr::from_ptr(ptr.cast_const()) }
        .to_string_lossy()
        .into_owned();
    unsafe { bdk_sys::bdkffi_free(ptr.cast()) };
    asm
}

fn len_to_c_int(len: usize) -> Result<c_int, ()> {
    if len > c_int::MAX as usize {
        Err(())
    } else {
        Ok(len as c_int)
    }
}
