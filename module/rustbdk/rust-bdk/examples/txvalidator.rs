mod support;

use std::env;
use std::error::Error;

use rust_bdk::TxValidator;

// Requires a prior `cmake --build` so the local `libbdkffi` archive exists.

struct Args {
    consensus: bool,
}

fn main() -> Result<(), Box<dyn Error>> {
    let args = Args::parse()?;
    let extended_tx = support::decode_hex(support::MAINNET_TX_HEX_EXTENDED)?;
    let validator = TxValidator::new(support::MAINNET_NETWORK).ok_or_else(|| {
        support::invalid_data(format!("unknown network {}", support::MAINNET_NETWORK))
    })?;

    validator.validate_transaction(
        &extended_tx,
        support::MAINNET_UTXO_HEIGHTS,
        support::MAINNET_BLOCK_HEIGHT,
        args.consensus,
    )?;

    println!("network: {}", support::MAINNET_NETWORK);
    println!("consensus: {}", args.consensus);
    println!("txid: {}", support::MAINNET_TX_ID);
    println!("block height: {}", support::MAINNET_BLOCK_HEIGHT);
    println!("extended tx bytes: {}", extended_tx.len());
    println!("validation: ok");

    Ok(())
}

impl Args {
    fn parse() -> Result<Self, Box<dyn Error>> {
        let mut consensus = true;
        let args = env::args().skip(1);

        for arg in args {
            match arg.as_str() {
                "-c" | "--disable-consensus" => consensus = false,
                "-h" | "--help" => {
                    print_usage();
                    std::process::exit(0);
                }
                other => {
                    return Err(support::invalid_data(format!("unknown argument {other}")));
                }
            }
        }

        Ok(Self { consensus })
    }
}

fn print_usage() {
    println!("cargo run --example txvalidator -- [--disable-consensus]");
}
