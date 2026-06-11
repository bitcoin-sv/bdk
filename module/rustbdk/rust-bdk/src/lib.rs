//! Safe Rust wrappers over the `bdkffi_` C ABI.

pub mod asm;
pub mod error;
pub mod txvalidator;
pub mod validatebatch;
pub mod version;

pub use asm::{from_asm, to_asm};
pub use error::{DosError, ScriptError, TxError};
pub use txvalidator::{ProtocolEra, TxValidator};
pub use validatebatch::ValidateBatch;
pub use version::*;
