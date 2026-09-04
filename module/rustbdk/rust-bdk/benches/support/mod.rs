#![allow(dead_code, unused_imports)]

use rust_bdk::{TxValidator, ValidateBatch};

// Provenance: module/example/bench_validatetransaction_single.cpp.
pub const NETWORK: &str = "main";
pub const BLOCK_HEIGHT: i32 = 620_940;
pub const UTXO_HEIGHTS: &[i32] = &[574_441];
pub const TX_HEX_EXTENDED: &str = "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000";

pub fn validator() -> TxValidator {
    TxValidator::new(NETWORK).unwrap_or_else(|| panic!("failed to create validator for {NETWORK}"))
}

pub fn validation_tx() -> Vec<u8> {
    decode_hex(TX_HEX_EXTENDED)
}

pub fn validation_batch(tx: &[u8], entries: usize) -> ValidateBatch {
    let mut batch = ValidateBatch::with_capacity(entries);
    for _ in 0..entries {
        batch.add(tx, UTXO_HEIGHTS, BLOCK_HEIGHT, true);
    }
    batch
}

pub fn deterministic_bytes(len: usize) -> Vec<u8> {
    (0..len)
        .map(|i| {
            let mixed = i.wrapping_mul(31).wrapping_add(i >> 3).wrapping_add(17);
            mixed as u8
        })
        .collect()
}

fn decode_hex(hex: &str) -> Vec<u8> {
    let trimmed = hex.trim();
    assert!(
        trimmed.len() % 2 == 0,
        "hex string has odd length: {}",
        trimmed.len()
    );

    trimmed
        .as_bytes()
        .as_chunks::<2>()
        .0
        .iter()
        .map(|pair| (hex_nibble(pair[0]) << 4) | hex_nibble(pair[1]))
        .collect()
}

fn hex_nibble(byte: u8) -> u8 {
    match byte {
        b'0'..=b'9' => byte - b'0',
        b'a'..=b'f' => byte - b'a' + 10,
        b'A'..=b'F' => byte - b'A' + 10,
        _ => panic!("invalid hex byte: {byte}"),
    }
}
