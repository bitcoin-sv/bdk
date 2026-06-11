mod support;

use std::env;
use std::error::Error;

use rust_bdk::{from_asm, to_asm};

// Requires a prior `cmake --build` so the local `libbdkffi` archive exists.
//
// The C++ example builds and signs full transactions that execute these
// opcodes. The safe Rust crate does not expose transaction construction or
// signing, so this example is intentionally reduced to tracked opcode-fragment
// assembly and exact byte assertions through the public asm wrapper.

struct OpcodeCase {
    key: &'static str,
    name: &'static str,
    description: &'static str,
    script_pubkey_fragment: &'static str,
    expected_bytes: &'static [u8],
}

const CASES: &[OpcodeCase] = &[
    // These fragments mirror the C++ `BuildOp*` opcodeCheck builders in
    // module/example/example_special_opcode_validate.cpp.
    OpcodeCase {
        key: "cat",
        name: "OP_CAT",
        description: "concatenate two byte arrays",
        script_pubkey_fragment: "OP_CAT 0x04 0x42535621 OP_EQUALVERIFY",
        expected_bytes: &[0x7e, 0x04, 0x42, 0x53, 0x56, 0x21, 0x88],
    },
    OpcodeCase {
        key: "num2bin",
        name: "OP_NUM2BIN",
        description: "encode integer as fixed-width bytes",
        script_pubkey_fragment: "OP_NUM2BIN 0x04 0x07000000 OP_EQUALVERIFY",
        expected_bytes: &[0x80, 0x04, 0x07, 0x00, 0x00, 0x00, 0x88],
    },
    OpcodeCase {
        key: "bin2num",
        name: "OP_BIN2NUM",
        description: "decode little-endian bytes as a script number",
        script_pubkey_fragment: "OP_BIN2NUM 7 OP_EQUALVERIFY",
        expected_bytes: &[0x81, 0x57, 0x88],
    },
    OpcodeCase {
        key: "and",
        name: "OP_AND",
        description: "bitwise and",
        script_pubkey_fragment: "OP_AND 0x01 0x0f OP_EQUALVERIFY",
        expected_bytes: &[0x84, 0x01, 0x0f, 0x88],
    },
    OpcodeCase {
        key: "mul",
        name: "OP_MUL",
        description: "multiply two integers",
        script_pubkey_fragment: "OP_MUL 12 OP_EQUALVERIFY",
        expected_bytes: &[0x95, 0x5c, 0x88],
    },
    OpcodeCase {
        key: "2mul",
        name: "OP_2MUL",
        description: "multiply integer by two after Chronicle",
        script_pubkey_fragment: "OP_2MUL 10 OP_EQUALVERIFY",
        expected_bytes: &[0x8d, 0x5a, 0x88],
    },
];

fn main() -> Result<(), Box<dyn Error>> {
    let selection = parse_selection()?;
    let selected: Vec<_> = CASES
        .iter()
        .filter(|case| match selection.as_deref() {
            Some(key) => key == case.key,
            None => true,
        })
        .collect();

    if selected.is_empty() {
        return Err(support::invalid_data("no opcode cases selected"));
    }

    for case in selected {
        let script = from_asm(case.script_pubkey_fragment);
        if script.as_slice() != case.expected_bytes {
            return Err(support::invalid_data(format!(
                "{} assembled to {}, expected {}",
                case.name,
                support::encode_hex(&script),
                support::encode_hex(case.expected_bytes)
            )));
        }

        println!("{}: {}", case.name, case.description);
        println!("  asm: {}", to_asm(&script));
        println!("  hex: {}", support::encode_hex(&script));
    }

    Ok(())
}

fn parse_selection() -> Result<Option<String>, Box<dyn Error>> {
    let mut args = env::args().skip(1);
    let mut selection = None;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "-o" | "--opcode" => {
                selection = Some(
                    args.next()
                        .ok_or_else(|| support::invalid_data("missing --opcode value"))?,
                );
            }
            "-h" | "--help" => {
                print_usage();
                std::process::exit(0);
            }
            other => return Err(support::invalid_data(format!("unknown argument {other}"))),
        }
    }

    if let Some(key) = selection.as_deref() {
        if !CASES.iter().any(|case| case.key == key) {
            return Err(support::invalid_data(format!("unknown opcode key {key}")));
        }
    }

    Ok(selection)
}

fn print_usage() {
    println!("cargo run --example special_opcode -- [--opcode cat|num2bin|bin2num|and|mul|2mul]");
}
