//! The process-wide helper limit for parallel input verification, through the
//! Rust binding. This file builds into its own test binary, so the setting
//! starts at its default here; one test function, because Cargo runs the tests
//! of one binary on parallel threads and the setting is shared.

mod common;

use common::decode_hex;
use rust_bdk::{TxValidator, parallel_input_helpers, set_parallel_input_helpers};

// Provenance: generated with test/golang/ParallelInputHelpers_test.go's
// makeHelpersP2PKHTx: 32 P2PKH inputs signed by private key 1, SIGHASH_ALL |
// FORKID; in the second, input 20 carries a signature of the wrong message.
const VALID_TX_HEX: &str = include_str!("data/p2pkh_32_inputs.hex");
const BROKEN_TX_HEX: &str = include_str!("data/p2pkh_32_inputs_bad_sig_at_20.hex");
const N_INPUTS: usize = 32;
const UTXO_HEIGHT: i32 = 699_999;
const BLOCK_HEIGHT: i32 = 700_000;

#[test]
fn helper_limit_reads_back_and_keeps_results() {
    assert_eq!(parallel_input_helpers(), -1, "defaults to automatic");
    for helpers in [0, 1, 3, 1000, -1] {
        set_parallel_input_helpers(helpers);
        assert_eq!(parallel_input_helpers(), helpers);
    }
    set_parallel_input_helpers(-7);
    assert_eq!(
        parallel_input_helpers(),
        -1,
        "every negative value means automatic"
    );

    let validator = TxValidator::new("main").expect("known BDK network");
    let valid = decode_hex(VALID_TX_HEX);
    let broken = decode_hex(BROKEN_TX_HEX);
    let heights = vec![UTXO_HEIGHT; N_INPUTS];
    assert_eq!(
        validator.validate_transaction(&valid, &heights, BLOCK_HEIGHT, true),
        Ok(())
    );
    let want = validator
        .validate_transaction(&broken, &heights, BLOCK_HEIGHT, true)
        .expect_err("input 20 has a bad signature");
    for helpers in [0, 1, 2, -1] {
        set_parallel_input_helpers(helpers);
        assert_eq!(
            validator.validate_transaction(&valid, &heights, BLOCK_HEIGHT, true),
            Ok(()),
            "helpers={helpers}"
        );
        assert_eq!(
            validator.validate_transaction(&broken, &heights, BLOCK_HEIGHT, true),
            Err(want.clone()),
            "helpers={helpers}"
        );
    }
}
