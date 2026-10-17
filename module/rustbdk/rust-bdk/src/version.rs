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

/// UTC time of the configure that first saw the current source revision (the BDK
/// and bitcoin-sv commits, including their `_dirty` state at that configure), or
/// `SOURCE_DATE_EPOCH` when the build set it. The `_dirty` suffix on
/// [`source_git_commit_hash`] reflects the tree at the last configure, not edits made
/// after it.
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

/// The SHA-256 transforms BDK hashes with on this CPU, e.g. `shani,shani(2way)`,
/// `sse4`, `arm_shani,arm_shani(2way)` or `standard`, and `standard(forced)` when
/// `BDK_SHA256_FORCE_SCALAR` is set. The first call, or the first [`TxValidator`],
/// makes the selection, so calling it at startup before any validation is safe and
/// reports what validation will use.
///
/// [`TxValidator`]: crate::TxValidator
pub fn sha256_backend() -> &'static str {
    // The C++ selection keeps its string for the life of the process and never frees it
    static_str(bdk_sys::bdkffi_sha256_backend)
}

fn static_str(ffi: unsafe extern "C" fn() -> *const c_char) -> &'static str {
    // Boundary contract: these externs return pointers that stay valid for the
    // life of the process (string literals from capi/src/version.cpp.in, and the
    // never-freed SHA-256 selection). Unlike error strings, they are not
    // malloc-owned buffers and must not be copied-and-freed with bdkffi_free.
    let ptr = unsafe { ffi() };
    if ptr.is_null() {
        return "";
    }

    unsafe { CStr::from_ptr(ptr) }.to_str().unwrap_or("")
}
