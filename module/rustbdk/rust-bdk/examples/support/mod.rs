#![allow(dead_code)]

use std::error::Error;
use std::io;

pub const MAINNET_NETWORK: &str = "main";
pub const MAINNET_BLOCK_HEIGHT: i32 = 632_099;
pub const MAINNET_TX_ID: &str = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
// Provenance: test/core/test_txvalidator.cpp `test_verify_script`.
pub const MAINNET_TX_HEX_EXTENDED: &str = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
pub const MAINNET_UTXO_HEIGHTS: &[i32] = &[631_924, 631_924];

pub const BENCH_NETWORK: &str = "main";
pub const BENCH_BLOCK_HEIGHT: i32 = 620_940;
pub const BENCH_TX_ID: &str = "d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7";
// Provenance: module/example/bench_validatetransaction.cpp `TX_HEX_EXTENDED`.
pub const BENCH_TX_HEX_EXTENDED: &str = "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000";
pub const BENCH_UTXO_HEIGHTS: &[i32] = &[574_441];

pub fn decode_hex(hex: &str) -> Result<Vec<u8>, Box<dyn Error>> {
    let trimmed = hex.trim();
    if trimmed.len() % 2 != 0 {
        return Err(invalid_data(format!(
            "hex string has odd length: {}",
            trimmed.len()
        )));
    }

    trimmed
        .as_bytes()
        .chunks_exact(2)
        .map(|pair| Ok((hex_nibble(pair[0])? << 4) | hex_nibble(pair[1])?))
        .collect()
}

pub fn encode_hex(bytes: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut out = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        out.push(HEX[(byte >> 4) as usize] as char);
        out.push(HEX[(byte & 0x0f) as usize] as char);
    }
    out
}

pub fn invalid_data(message: impl Into<String>) -> Box<dyn Error> {
    Box::new(io::Error::new(io::ErrorKind::InvalidData, message.into()))
}

fn hex_nibble(byte: u8) -> Result<u8, Box<dyn Error>> {
    match byte {
        b'0'..=b'9' => Ok(byte - b'0'),
        b'a'..=b'f' => Ok(byte - b'a' + 10),
        b'A'..=b'F' => Ok(byte - b'A' + 10),
        _ => Err(invalid_data(format!("invalid hex byte: {byte}"))),
    }
}
