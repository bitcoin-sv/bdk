//! Safe Rust wrappers over the `bdkffi_` C ABI.

pub mod error;
pub mod txvalidator;
pub mod version;

pub use error::{DosError, ScriptError, TxError};
pub use txvalidator::{ProtocolEra, TxValidator};
pub use version::*;
