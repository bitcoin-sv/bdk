use std::ffi::CStr;
use std::fmt;
use std::os::raw::{c_char, c_int};

/// Script execution errors mirrored from bitcoin-sv `script/script_error.h`.
///
/// `SCRIPT_ERR_ERROR_COUNT` is exposed as [`ScriptError::COUNT`], not as an
/// enum variant. Unknown numeric values are preserved in [`ScriptError::Unknown`]
/// for forward compatibility.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ScriptError {
    Ok,
    UnknownError,
    EvalFalse,
    OpReturn,
    ScriptSize,
    PushSize,
    OpCount,
    StackSize,
    SigCount,
    PubkeyCount,
    InvalidOperandSize,
    InvalidNumberRange,
    ImpossibleEncoding,
    InvalidSplitRange,
    ScriptnumOverflow,
    ScriptnumMinencode,
    Verify,
    EqualVerify,
    CheckMultiSigVerify,
    CheckSigVerify,
    NumEqualVerify,
    BadOpcode,
    DisabledOpcode,
    InvalidStackOperation,
    InvalidAltStackOperation,
    UnbalancedConditional,
    DivByZero,
    ModByZero,
    NegativeLockTime,
    UnsatisfiedLockTime,
    SigHashType,
    SigDer,
    MinimalData,
    SigPushOnly,
    SigHighS,
    SigNullDummy,
    PubkeyType,
    CleanStack,
    MinimalIf,
    SigNullFail,
    DiscourageUpgradableNops,
    NonCompressedPubKey,
    IllegalForkId,
    MustUseForkId,
    IllegalChronicle,
    BigInt,
    InvalidFlags,
    Unknown(i32),
}

impl ScriptError {
    pub const COUNT: i32 = 47;

    pub fn from_code(code: i32) -> Self {
        match code {
            0 => Self::Ok,
            1 => Self::UnknownError,
            2 => Self::EvalFalse,
            3 => Self::OpReturn,
            4 => Self::ScriptSize,
            5 => Self::PushSize,
            6 => Self::OpCount,
            7 => Self::StackSize,
            8 => Self::SigCount,
            9 => Self::PubkeyCount,
            10 => Self::InvalidOperandSize,
            11 => Self::InvalidNumberRange,
            12 => Self::ImpossibleEncoding,
            13 => Self::InvalidSplitRange,
            14 => Self::ScriptnumOverflow,
            15 => Self::ScriptnumMinencode,
            16 => Self::Verify,
            17 => Self::EqualVerify,
            18 => Self::CheckMultiSigVerify,
            19 => Self::CheckSigVerify,
            20 => Self::NumEqualVerify,
            21 => Self::BadOpcode,
            22 => Self::DisabledOpcode,
            23 => Self::InvalidStackOperation,
            24 => Self::InvalidAltStackOperation,
            25 => Self::UnbalancedConditional,
            26 => Self::DivByZero,
            27 => Self::ModByZero,
            28 => Self::NegativeLockTime,
            29 => Self::UnsatisfiedLockTime,
            30 => Self::SigHashType,
            31 => Self::SigDer,
            32 => Self::MinimalData,
            33 => Self::SigPushOnly,
            34 => Self::SigHighS,
            35 => Self::SigNullDummy,
            36 => Self::PubkeyType,
            37 => Self::CleanStack,
            38 => Self::MinimalIf,
            39 => Self::SigNullFail,
            40 => Self::DiscourageUpgradableNops,
            41 => Self::NonCompressedPubKey,
            42 => Self::IllegalForkId,
            43 => Self::MustUseForkId,
            44 => Self::IllegalChronicle,
            45 => Self::BigInt,
            46 => Self::InvalidFlags,
            other => Self::Unknown(other),
        }
    }

    pub fn code(self) -> i32 {
        match self {
            Self::Ok => 0,
            Self::UnknownError => 1,
            Self::EvalFalse => 2,
            Self::OpReturn => 3,
            Self::ScriptSize => 4,
            Self::PushSize => 5,
            Self::OpCount => 6,
            Self::StackSize => 7,
            Self::SigCount => 8,
            Self::PubkeyCount => 9,
            Self::InvalidOperandSize => 10,
            Self::InvalidNumberRange => 11,
            Self::ImpossibleEncoding => 12,
            Self::InvalidSplitRange => 13,
            Self::ScriptnumOverflow => 14,
            Self::ScriptnumMinencode => 15,
            Self::Verify => 16,
            Self::EqualVerify => 17,
            Self::CheckMultiSigVerify => 18,
            Self::CheckSigVerify => 19,
            Self::NumEqualVerify => 20,
            Self::BadOpcode => 21,
            Self::DisabledOpcode => 22,
            Self::InvalidStackOperation => 23,
            Self::InvalidAltStackOperation => 24,
            Self::UnbalancedConditional => 25,
            Self::DivByZero => 26,
            Self::ModByZero => 27,
            Self::NegativeLockTime => 28,
            Self::UnsatisfiedLockTime => 29,
            Self::SigHashType => 30,
            Self::SigDer => 31,
            Self::MinimalData => 32,
            Self::SigPushOnly => 33,
            Self::SigHighS => 34,
            Self::SigNullDummy => 35,
            Self::PubkeyType => 36,
            Self::CleanStack => 37,
            Self::MinimalIf => 38,
            Self::SigNullFail => 39,
            Self::DiscourageUpgradableNops => 40,
            Self::NonCompressedPubKey => 41,
            Self::IllegalForkId => 42,
            Self::MustUseForkId => 43,
            Self::IllegalChronicle => 44,
            Self::BigInt => 45,
            Self::InvalidFlags => 46,
            Self::Unknown(code) => code,
        }
    }

    pub fn message(self) -> String {
        message_from_ffi(bdk_sys::bdkffi_script_error_string, self.code())
    }
}

impl fmt::Display for ScriptError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message())
    }
}

impl std::error::Error for ScriptError {}

/// Transaction validation errors mirrored from `core/doserror.hpp`.
///
/// `DoSError_t::Count` is exposed as [`DosError::COUNT`], not as an enum
/// variant. Unknown numeric values are preserved in [`DosError::Unknown`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum DosError {
    Ok,
    NullPrevout,
    P2SHOutputPostGenesis,
    SigopsConsensus,
    SigopsPolicy,
    NotFreeConsolidation,
    NotStandard,
    VinEmpty,
    VoutEmpty,
    Oversize,
    OutputNegative,
    OutputTooLarge,
    OutputTotalTooLarge,
    CoinbaseNotAllowed,
    DuplicateInputs,
    UnconfirmedInputInBlock,
    InputValuesOutOfRange,
    InputsBelowOutputs,
    InsufficientFee,
    Unknown(i32),
}

impl DosError {
    pub const COUNT: i32 = 19;

    pub fn from_code(code: i32) -> Self {
        match code {
            0 => Self::Ok,
            1 => Self::NullPrevout,
            2 => Self::P2SHOutputPostGenesis,
            3 => Self::SigopsConsensus,
            4 => Self::SigopsPolicy,
            5 => Self::NotFreeConsolidation,
            6 => Self::NotStandard,
            7 => Self::VinEmpty,
            8 => Self::VoutEmpty,
            9 => Self::Oversize,
            10 => Self::OutputNegative,
            11 => Self::OutputTooLarge,
            12 => Self::OutputTotalTooLarge,
            13 => Self::CoinbaseNotAllowed,
            14 => Self::DuplicateInputs,
            15 => Self::UnconfirmedInputInBlock,
            16 => Self::InputValuesOutOfRange,
            17 => Self::InputsBelowOutputs,
            18 => Self::InsufficientFee,
            other => Self::Unknown(other),
        }
    }

    pub fn code(self) -> i32 {
        match self {
            Self::Ok => 0,
            Self::NullPrevout => 1,
            Self::P2SHOutputPostGenesis => 2,
            Self::SigopsConsensus => 3,
            Self::SigopsPolicy => 4,
            Self::NotFreeConsolidation => 5,
            Self::NotStandard => 6,
            Self::VinEmpty => 7,
            Self::VoutEmpty => 8,
            Self::Oversize => 9,
            Self::OutputNegative => 10,
            Self::OutputTooLarge => 11,
            Self::OutputTotalTooLarge => 12,
            Self::CoinbaseNotAllowed => 13,
            Self::DuplicateInputs => 14,
            Self::UnconfirmedInputInBlock => 15,
            Self::InputValuesOutOfRange => 16,
            Self::InputsBelowOutputs => 17,
            Self::InsufficientFee => 18,
            Self::Unknown(code) => code,
        }
    }

    pub fn message(self) -> String {
        message_from_ffi(bdk_sys::bdkffi_dos_error_string, self.code())
    }
}

impl fmt::Display for DosError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message())
    }
}

impl std::error::Error for DosError {}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TxError {
    Script(ScriptError),
    DoS(DosError),
    Exception,
}

impl TxError {
    pub fn message(&self) -> String {
        match self {
            Self::Script(err) => err.message(),
            Self::DoS(err) => err.message(),
            Self::Exception => "C++ exception".to_owned(),
        }
    }
}

impl fmt::Display for TxError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Script(err) => write!(f, "script error {}: {}", err.code(), err),
            Self::DoS(err) => write!(f, "dos error {}: {}", err.code(), err),
            Self::Exception => f.write_str("C++ exception"),
        }
    }
}

impl std::error::Error for TxError {}

pub(crate) fn translate(raw: bdk_sys::TxError) -> Result<(), TxError> {
    match raw.domain {
        bdk_sys::TX_ERR_DOMAIN_OK => Ok(()),
        bdk_sys::TX_ERR_DOMAIN_SCRIPT => Err(TxError::Script(ScriptError::from_code(raw.code))),
        bdk_sys::TX_ERR_DOMAIN_DOS => Err(TxError::DoS(DosError::from_code(raw.code))),
        bdk_sys::TX_ERR_DOMAIN_EXCEPTION => Err(TxError::Exception),
        _ => Err(TxError::Exception),
    }
}

pub(crate) fn take_owned_c_string(ptr: *mut c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }

    let message = unsafe { CStr::from_ptr(ptr.cast_const()) }
        .to_string_lossy()
        .into_owned();
    unsafe { bdk_sys::bdkffi_free(ptr.cast()) };
    message
}

fn message_from_ffi(ffi: unsafe extern "C" fn(c_int) -> *mut c_char, code: i32) -> String {
    let ptr = unsafe { ffi(code as c_int) };
    take_owned_c_string(ptr)
}

#[cfg(test)]
mod tests {
    use super::ScriptError;

    // Stage 4 authors the enum drift guard only. Full validate/verify
    // happy/failing data-vector behaviour tests are deferred to Stage 7, where
    // the shared data-vector harness is introduced.
    #[test]
    fn script_error_enum_count_matches_cpp() {
        assert_eq!(
            ScriptError::COUNT as i32,
            unsafe { bdk_sys::bdkffi_cpp_script_err_error_count() }
        );
    }
}
