use std::ffi::CStr;
use std::os::raw::c_char;

pub fn bsv_client_version_major() -> i32 {
    unsafe { bdk_sys::bdkffi_bsv_client_version_major() as i32 }
}

pub fn bsv_client_version_minor() -> i32 {
    unsafe { bdk_sys::bdkffi_bsv_client_version_minor() as i32 }
}

pub fn bsv_client_version_revision() -> i32 {
    unsafe { bdk_sys::bdkffi_bsv_client_version_revision() as i32 }
}

pub fn bsv_version_string() -> &'static str {
    static_str(bdk_sys::bdkffi_bsv_version_string)
}

pub fn bsv_git_commit_tag_or_branch() -> &'static str {
    static_str(bdk_sys::bdkffi_bsv_git_commit_tag_or_branch)
}

pub fn bsv_git_commit_hash() -> &'static str {
    static_str(bdk_sys::bdkffi_bsv_git_commit_hash)
}

pub fn bsv_git_commit_datetime() -> &'static str {
    static_str(bdk_sys::bdkffi_bsv_git_commit_datetime)
}

pub fn bdk_version_major() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_version_major() as i32 }
}

pub fn bdk_version_minor() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_version_minor() as i32 }
}

pub fn bdk_version_patch() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_version_patch() as i32 }
}

pub fn bdk_version_string() -> &'static str {
    static_str(bdk_sys::bdkffi_bdk_version_string)
}

pub fn source_git_commit_tag_or_branch() -> &'static str {
    static_str(bdk_sys::bdkffi_source_git_commit_tag_or_branch)
}

pub fn source_git_commit_hash() -> &'static str {
    static_str(bdk_sys::bdkffi_source_git_commit_hash)
}

pub fn source_git_commit_datetime() -> &'static str {
    static_str(bdk_sys::bdkffi_source_git_commit_datetime)
}

pub fn bdk_build_datetime_utc() -> &'static str {
    static_str(bdk_sys::bdkffi_bdk_build_datetime_utc)
}

pub fn bdk_rust_version_major() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_rust_version_major() as i32 }
}

pub fn bdk_rust_version_minor() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_rust_version_minor() as i32 }
}

pub fn bdk_rust_version_patch() -> i32 {
    unsafe { bdk_sys::bdkffi_bdk_rust_version_patch() as i32 }
}

pub fn bdk_rust_version_string() -> &'static str {
    static_str(bdk_sys::bdkffi_bdk_rust_version_string)
}

fn static_str(ffi: unsafe extern "C" fn() -> *const c_char) -> &'static str {
    // Boundary contract: version externs return pointers to static storage from
    // capi/src/version.cpp.in. Unlike error strings, these are not malloc-owned
    // buffers and must not be copied-and-freed with bdkffi_free.
    let ptr = unsafe { ffi() };
    if ptr.is_null() {
        return "";
    }

    unsafe { CStr::from_ptr(ptr) }.to_str().unwrap_or("")
}
