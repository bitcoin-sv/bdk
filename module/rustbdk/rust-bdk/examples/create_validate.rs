mod support;

use std::error::Error;

use rust_bdk::{TxValidator, ValidateBatch};

// Requires a prior `cmake --build` so the local `libbdkffi` archive exists.
//
// The C++ create+validate example signs and serializes a transaction with
// core-only key and transaction builders. The safe Rust crate intentionally
// exposes validation, not transaction construction, so this example creates the
// Rust validation input from the same serialized extended-transaction shape and
// validates it through the public safe API.

struct ValidationInput {
    network: &'static str,
    txid: &'static str,
    extended_tx: Vec<u8>,
    utxo_heights: Vec<i32>,
    block_height: i32,
    consensus: bool,
}

fn main() -> Result<(), Box<dyn Error>> {
    let input = ValidationInput {
        network: support::BENCH_NETWORK,
        txid: support::BENCH_TX_ID,
        extended_tx: support::decode_hex(support::BENCH_TX_HEX_EXTENDED)?,
        utxo_heights: support::BENCH_UTXO_HEIGHTS.to_vec(),
        block_height: support::BENCH_BLOCK_HEIGHT,
        consensus: true,
    };

    let validator = TxValidator::new(input.network)
        .ok_or_else(|| support::invalid_data(format!("unknown network {}", input.network)))?;
    validator.validate_transaction(
        &input.extended_tx,
        &input.utxo_heights,
        input.block_height,
        input.consensus,
    )?;

    let mut batch = ValidateBatch::with_capacity(1);
    batch.add(
        &input.extended_tx,
        &input.utxo_heights,
        input.block_height,
        input.consensus,
    );
    assert!(validator.validate_batch(&batch).into_iter().all(|r| r.is_ok()));

    println!("created validation input for {}", input.txid);
    println!("network: {}", input.network);
    println!("block height: {}", input.block_height);
    println!("extended tx bytes: {}", input.extended_tx.len());
    println!("single and batch validation: ok");

    Ok(())
}
