//! The SHA-256 backend getter. Cargo builds each file under `tests/` as its own
//! binary, so this process has constructed no validator when the test starts:
//! the getter must select the backend itself, and concurrent first calls must
//! agree.

use std::sync::{Arc, Barrier};
use std::thread;

use rust_bdk::{TxValidator, sha256_backend};

/// Which SHA-256 backend BDK should select here, without BDK's own detection
/// code: `/proc/cpuinfo` on Linux, and on Apple silicon a fixed expectation
/// rather than a probe. `None` when this platform has neither.
fn expected_sha256_backend() -> Option<&'static str> {
    if let Ok(v) = std::env::var("BDK_SHA256_FORCE_SCALAR") {
        if !v.is_empty() && v != "0" {
            return Some("standard(forced)");
        }
    }
    if cfg!(all(target_os = "macos", target_arch = "aarch64")) {
        return Some("arm_shani,arm_shani(2way)"); // every Apple silicon core implements FEAT_SHA256
    }
    if !cfg!(target_os = "linux") {
        return None;
    }
    let cpuinfo = std::fs::read_to_string("/proc/cpuinfo").ok()?;
    let line = cpuinfo
        .lines()
        .find(|l| l.starts_with("flags") || l.starts_with("Features"))?;
    let flags: Vec<&str> = line.split(':').nth(1)?.split_whitespace().collect();
    let has = |f: &str| flags.contains(&f);
    if cfg!(target_arch = "x86_64") {
        if has("sse4_1") && has("sha_ni") {
            return Some("shani,shani(2way)");
        }
        if has("sse4_1") {
            return Some("sse4");
        }
    }
    if cfg!(target_arch = "aarch64") && has("sha2") {
        return Some("arm_shani,arm_shani(2way)");
    }
    Some("standard")
}

#[test]
fn sha256_backend_is_selected_by_the_getter() {
    const CALLERS: usize = 16;
    let barrier = Arc::new(Barrier::new(CALLERS));
    let handles: Vec<_> = (0..CALLERS)
        .map(|_| {
            let barrier = Arc::clone(&barrier);
            thread::spawn(move || {
                barrier.wait();
                sha256_backend()
            })
        })
        .collect();
    let got: Vec<&'static str> = handles.into_iter().map(|h| h.join().unwrap()).collect();

    println!("SHA-256 backend: {}", got[0]);
    assert!(!got[0].is_empty());
    assert!(got.iter().all(|b| *b == got[0]), "callers disagree: {got:?}");
    if let Some(expected) = expected_sha256_backend() {
        assert_eq!(got[0], expected);
    }

    // Constructing a validator afterwards does not change the selection
    let _validator = TxValidator::new("main").expect("known BDK network");
    assert_eq!(sha256_backend(), got[0]);
}
