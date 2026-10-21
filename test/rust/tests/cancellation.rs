// Shared fixtures; this test uses only some of them.
#[allow(dead_code)]
mod common;

use std::sync::{Arc, Mutex, MutexGuard};
use std::thread;
use std::time::{Duration, Instant};

use rust_bdk::{CancelFlag, TxError, TxValidator, ValidateBatch};

// These integration tests link the local `libbdkffi` archive. Run
// `cmake --build` first so `module/rustbdk/bdk-sys/lib/libbdkffi_*.a` exists.

// Core policy setters write shared state; serialise the tests that use them.
static POLICY_LOCK: Mutex<()> = Mutex::new(());

fn policy_guard() -> MutexGuard<'static, ()> {
    POLICY_LOCK
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner())
}

// Regtest heights at which the spent output and the block are post-Chronicle.
const LONG_BLOCK_HEIGHT: i32 = 20_000;
const LONG_UTXO_HEIGHTS: &[i32] = &[19_000];

fn push_var_int(out: &mut Vec<u8>, n: u64) {
    match n {
        0..=0xfc => out.push(n as u8),
        0xfd..=0xffff => {
            out.push(0xfd);
            out.extend_from_slice(&(n as u16).to_le_bytes());
        }
        0x1_0000..=0xffff_ffff => {
            out.push(0xfe);
            out.extend_from_slice(&(n as u32).to_le_bytes());
        }
        _ => {
            out.push(0xff);
            out.extend_from_slice(&n.to_le_bytes());
        }
    }
}

// A valid one-input extended transaction whose script runs for seconds: the
// unlocking script pushes a 1 MB element and the locking script hashes it `rounds`
// times (OP_DUP OP_SHA256 OP_DROP).
fn long_running_tx(rounds: usize) -> Vec<u8> {
    let element = vec![0x01u8; 1 << 20];
    let mut script_sig = vec![0x4e]; // OP_PUSHDATA4
    script_sig.extend_from_slice(&(element.len() as u32).to_le_bytes());
    script_sig.extend_from_slice(&element);
    let locking_script: Vec<u8> = [0x76u8, 0xa8, 0x75].repeat(rounds);

    let mut tx = Vec::new();
    tx.extend_from_slice(&2u32.to_le_bytes());
    tx.extend_from_slice(&[0, 0, 0, 0, 0, 0xef]);
    push_var_int(&mut tx, 1);
    let mut prev_txid = [0u8; 32];
    prev_txid[0] = 1;
    tx.extend_from_slice(&prev_txid);
    tx.extend_from_slice(&0u32.to_le_bytes());
    push_var_int(&mut tx, script_sig.len() as u64);
    tx.extend_from_slice(&script_sig);
    tx.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
    tx.extend_from_slice(&100_000u64.to_le_bytes());
    push_var_int(&mut tx, locking_script.len() as u64);
    tx.extend_from_slice(&locking_script);
    push_var_int(&mut tx, 1);
    tx.extend_from_slice(&1_000u64.to_le_bytes());
    push_var_int(&mut tx, 1);
    tx.push(0x51); // OP_1
    tx.extend_from_slice(&0u32.to_le_bytes());
    tx
}

fn long_running_validator() -> TxValidator {
    let validator = TxValidator::new("regtest").expect("regtest validator");
    validator
        .set_max_script_size_policy(4 << 20)
        .expect("max script size");
    validator
        .set_max_ops_per_script_policy(1_000_000)
        .expect("max ops");
    validator
}

#[test]
fn cancel_flag_is_send_and_sync() {
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<CancelFlag>();
}

#[test]
fn cancelled_before_start_returns_cancelled_from_every_entry_point() {
    let _guard = policy_guard();
    let validator = TxValidator::new("main").expect("main validator");
    let valid = common::decode_hex(common::MAINNET_TX_HEX);
    let malformed = vec![0x01u8, 0x02, 0x03];
    let flag = CancelFlag::new();
    flag.cancel();
    assert!(flag.is_cancelled());

    for consensus in [true, false] {
        for tx in [&valid, &malformed] {
            let utxo = common::MAINNET_UTXO_HEIGHTS;
            let height = common::MAINNET_BLOCK_HEIGHT;
            assert_eq!(
                validator.validate_transaction_cancellable(tx, utxo, height, consensus, &flag),
                Err(TxError::Cancelled)
            );
            assert_eq!(
                validator.verify_script_cancellable(tx, utxo, height, consensus, &flag),
                Err(TxError::Cancelled)
            );
            assert_eq!(
                validator.verify_script_with_custom_flags_cancellable(
                    tx,
                    utxo,
                    height,
                    consensus,
                    common::MAINNET_CUSTOM_FLAGS,
                    &flag
                ),
                Err(TxError::Cancelled)
            );

            let mut batch = ValidateBatch::new();
            batch.add(tx, utxo, height, consensus);
            batch.add(tx, utxo, height, consensus);
            assert_eq!(
                validator.validate_batch_cancellable(&batch, &flag),
                vec![Err(TxError::Cancelled), Err(TxError::Cancelled)]
            );
        }
    }
}

// One entry per input of the mainnet fixture: P2SH evaluation without the
// post-Genesis flags. The fixture's inputs spend P2SH outputs whose redeem scripts
// the calculated (post-Genesis) flags never run; with these flags they fail.
const P2SH_ONLY_FLAGS: &[u32] = &[1, 1];

#[test]
fn unset_flag_gives_the_uncancelled_result() {
    let _guard = policy_guard();
    let validator = TxValidator::new("main").expect("main validator");
    let valid = common::decode_hex(common::MAINNET_TX_HEX);
    let mut invalid = valid.clone();
    invalid[60] ^= 0x01; // first input's script
    let utxo = common::MAINNET_UTXO_HEIGHTS;
    let height = common::MAINNET_BLOCK_HEIGHT;
    let flag = CancelFlag::new();

    // The fixtures keep the properties the comparisons rely on.
    assert!(matches!(
        validator.validate_transaction(&invalid, utxo, height, true),
        Err(TxError::Script(_))
    ));
    assert_eq!(validator.verify_script(&valid, utxo, height, true), Ok(()));
    assert!(matches!(
        validator.verify_script_with_custom_flags(&valid, utxo, height, true, P2SH_ONLY_FLAGS),
        Err(TxError::Script(_))
    ));

    for tx in [&valid, &invalid] {
        for consensus in [true, false] {
            assert_eq!(
                validator.validate_transaction(tx, utxo, height, consensus),
                validator.validate_transaction_cancellable(tx, utxo, height, consensus, &flag)
            );
            assert_eq!(
                validator.verify_script(tx, utxo, height, consensus),
                validator.verify_script_cancellable(tx, utxo, height, consensus, &flag)
            );
            for flags in [common::MAINNET_CUSTOM_FLAGS, P2SH_ONLY_FLAGS, &[]] {
                assert_eq!(
                    validator.verify_script_with_custom_flags(tx, utxo, height, consensus, flags),
                    validator.verify_script_with_custom_flags_cancellable(
                        tx, utxo, height, consensus, flags, &flag
                    ),
                    "flags={flags:?}"
                );
            }

            let mut batch = ValidateBatch::new();
            batch.add(&valid, utxo, height, consensus);
            batch.add(tx, utxo, height, consensus);
            assert_eq!(
                validator.validate_batch(&batch),
                validator.validate_batch_cancellable(&batch, &flag)
            );
        }
    }
}

#[test]
fn cancel_from_another_thread_stops_a_running_call() {
    let _guard = policy_guard();
    let validator = long_running_validator();
    let tx = long_running_tx(2000); // seconds uncancelled

    for consensus in [true, false] {
        let flag = Arc::new(CancelFlag::new());
        let canceller = {
            let flag = Arc::clone(&flag);
            thread::spawn(move || {
                thread::sleep(Duration::from_millis(50));
                flag.cancel();
            })
        };
        let started = Instant::now();
        let result = validator.validate_transaction_cancellable(
            &tx,
            LONG_UTXO_HEIGHTS,
            LONG_BLOCK_HEIGHT,
            consensus,
            &flag,
        );
        let elapsed = started.elapsed();
        canceller.join().expect("canceller thread");

        assert_eq!(result, Err(TxError::Cancelled), "consensus={consensus}");
        assert!(
            elapsed < Duration::from_secs(1),
            "consensus={consensus}: took {elapsed:?}"
        );
    }
    assert_eq!(TxError::Cancelled.to_string(), "validation cancelled");
}
