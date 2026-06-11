//! Independent linkage test for the built rust-bdk library.
//!
//! This mirrors the intent of `test/golang`: it is a *separate* Cargo package
//! that consumes `rust-bdk` as an external path dependency. The source lives in
//! the same repository, but the test is built and linked as if by an outside
//! consumer. If this compiles and the test passes, the whole native link
//! closure has resolved — the merged `libbdkffi_<os_arch>.a` archive, `bdk_core`,
//! the BSV application sources, and the C++ runtime are all wired up correctly.
//!
//! It deliberately exercises only a few fundamental calls (construction, version
//! externs, an asm round-trip, the error path) — enough to prove linkage, not a
//! full behaviour suite (that lives under `module/rustbdk/rust-bdk/tests`).

use rust_bdk::{TxValidator, bdk_rust_version_string, bsv_version_string, from_asm, to_asm};

#[test]
fn rust_bdk_links_and_basic_calls_work() {
    // Constructing a validator for a known network exercises the C++ shim and
    // the archive link (bdkffi_txvalidator_create -> bsv::CTxValidator).
    let validator = TxValidator::new("main").expect("known BDK network");
    assert_eq!(validator.genesis_activation_height(), 620_538);

    // Version strings resolve through the FFI version externs.
    assert!(!bsv_version_string().is_empty());
    assert!(!bdk_rust_version_string().is_empty());

    // asm encode/decode round-trips by byte idempotency.
    let script = from_asm("4 5 ADD 9 EQUAL");
    assert_eq!(from_asm(&to_asm(&script)), script);

    // An unknown network returns None via the shim's exception-to-null path.
    assert!(TxValidator::new("definitely-not-a-network").is_none());
}
