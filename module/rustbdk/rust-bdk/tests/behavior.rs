mod common;

use std::sync::{Mutex, MutexGuard};

use rust_bdk::{
    bdk_rust_version_major, bdk_rust_version_minor, bdk_rust_version_patch,
    bdk_rust_version_string, bdk_version_major, bdk_version_minor, bdk_version_patch,
    bdk_version_string, bsv_client_version_major, bsv_client_version_minor,
    bsv_client_version_revision, bsv_version_string, ProtocolEra, ScriptError, TxError, TxValidator,
    ValidateBatch,
};

// These integration tests link the local `libbdkffi` archive. Run
// `cmake --build` first so `module/rustbdk/bdk-sys/lib/libbdkffi_*.a` exists.

static POLICY_LOCK: Mutex<()> = Mutex::new(());

const ONE_KILOBYTE: u64 = 1_000;
const ONE_MEGABYTE: u64 = ONE_KILOBYTE * 1_000;
const DEFAULT_OPS_PER_SCRIPT_POLICY_AFTER_GENESIS: u64 = u32::MAX as u64;
const DEFAULT_SCRIPT_NUM_LENGTH_POLICY: u64 = 10 * ONE_KILOBYTE;
const DEFAULT_MAX_SCRIPT_SIZE_POLICY_AFTER_GENESIS: u64 = 500 * ONE_KILOBYTE;
const DEFAULT_PUBKEYS_PER_MULTISIG_POLICY_AFTER_GENESIS: u64 = u32::MAX as u64;
const DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS: u64 = 100 * ONE_MEGABYTE;
const DEFAULT_STACK_MEMORY_USAGE_CONSENSUS_AFTER_GENESIS: u64 = i64::MAX as u64;
const DEFAULT_MAX_TX_SIZE_POLICY_AFTER_GENESIS: u64 = 10 * ONE_MEGABYTE;
const DEFAULT_DATA_CARRIER_SIZE: u64 = u32::MAX as u64;

#[test]
fn txvalidator_networks_match_go_activation_heights() {
    let _guard = policy_guard();
    let cases = [
        ("main", 620_538, 943_816),
        ("test", 1_344_302, 1_713_168),
        ("regtest", 10_000, 15_000),
        ("stn", 100, 250),
        ("teratestnet", 1, 2),
        ("tstn", 1, 2),
    ];

    for (network, genesis, chronicle) in cases {
        let validator = validator(network);
        assert_eq!(validator.genesis_activation_height(), genesis, "{network}");
        assert_eq!(validator.chronicle_activation_height(), chronicle, "{network}");
    }

    assert!(TxValidator::new("foo").is_none());
}

#[test]
fn txvalidator_policy_settings_match_go_wrappers() {
    let _guard = policy_guard();
    let validator = validator("main");

    validator.set_max_ops_per_script_policy(1_000).unwrap();
    assert_eq!(
        validator.max_ops_per_script(ProtocolEra::PostGenesis, false),
        1_000
    );

    validator.set_max_script_num_length_policy(1_000).unwrap();
    assert_eq!(
        validator.max_script_num_length(ProtocolEra::PostGenesis, false),
        1_000
    );

    validator.set_max_script_size_policy(1_000).unwrap();
    assert_eq!(
        validator.max_script_size(ProtocolEra::PostGenesis, false),
        1_000
    );

    validator.set_max_pub_keys_per_multisig_policy(1_000).unwrap();
    assert_eq!(
        validator.max_pub_keys_per_multisig(ProtocolEra::PostGenesis, false),
        1_000
    );

    validator.set_max_stack_memory_usage(2_000, 1_000).unwrap();
    assert_eq!(
        validator.max_stack_memory_usage(ProtocolEra::PostGenesis, false),
        1_000
    );

    validator.set_genesis_activation_height(1_000).unwrap();
    assert_eq!(validator.genesis_activation_height(), 1_000);

    validator.set_chronicle_activation_height(1_000).unwrap();
    assert_eq!(validator.chronicle_activation_height(), 1_000);

    validator.set_genesis_graceful_period(50).unwrap();
    assert_eq!(validator.genesis_graceful_period(), 50);

    validator.set_chronicle_graceful_period(50).unwrap();
    assert_eq!(validator.chronicle_graceful_period(), 50);

    validator.set_max_tx_size_policy(1024 * 1024).unwrap();
    assert_eq!(
        validator.max_tx_size(ProtocolEra::PostGenesis, false),
        1024 * 1024
    );

    validator.set_data_carrier_size(512);
    assert_eq!(validator.data_carrier_size(), 512);

    validator.set_data_carrier(false);
    assert!(!validator.data_carrier());
    validator.set_data_carrier(true);
    assert!(validator.data_carrier());

    validator.set_accept_non_standard_output(false);
    assert!(!validator.accept_non_standard_output(ProtocolEra::PostGenesis));
    validator.set_accept_non_standard_output(true);
    assert!(validator.accept_non_standard_output(ProtocolEra::PostGenesis));

    validator.set_require_standard(false);
    assert!(!validator.require_standard());
    validator.set_require_standard(true);
    assert!(validator.require_standard());

    validator.set_permit_bare_multisig(false);
    assert!(!validator.permit_bare_multisig());
    validator.set_permit_bare_multisig(true);
    assert!(validator.permit_bare_multisig());

    validator.reset_default();
    assert_eq!(
        validator.max_ops_per_script(ProtocolEra::PostGenesis, false),
        DEFAULT_OPS_PER_SCRIPT_POLICY_AFTER_GENESIS
    );
    assert_eq!(
        validator.max_script_num_length(ProtocolEra::PostGenesis, false),
        DEFAULT_SCRIPT_NUM_LENGTH_POLICY
    );
    assert_eq!(
        validator.max_script_size(ProtocolEra::PostGenesis, false),
        DEFAULT_MAX_SCRIPT_SIZE_POLICY_AFTER_GENESIS
    );
    assert_eq!(
        validator.max_pub_keys_per_multisig(ProtocolEra::PostGenesis, false),
        DEFAULT_PUBKEYS_PER_MULTISIG_POLICY_AFTER_GENESIS
    );
    assert_eq!(
        validator.max_stack_memory_usage(ProtocolEra::PostGenesis, false),
        DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS
    );
    assert_eq!(
        validator.max_stack_memory_usage(ProtocolEra::PostGenesis, true),
        DEFAULT_STACK_MEMORY_USAGE_CONSENSUS_AFTER_GENESIS
    );
    assert_eq!(
        validator.max_tx_size(ProtocolEra::PostGenesis, false),
        DEFAULT_MAX_TX_SIZE_POLICY_AFTER_GENESIS
    );
    assert_eq!(validator.data_carrier_size(), DEFAULT_DATA_CARRIER_SIZE);
    assert!(validator.data_carrier());
    assert!(validator.require_standard());
    assert!(validator.permit_bare_multisig());
    assert_eq!(validator.genesis_graceful_period(), 72);
    assert_eq!(validator.chronicle_graceful_period(), 72);

    restore_mainnet_activation_heights(&validator);
}

#[test]
fn txvalidator_validates_tracked_vectors() {
    let _guard = policy_guard();
    let tx = common::decode_hex(common::MAINNET_TX_HEX);
    let validator = validator("main");

    validator
        .validate_transaction(&tx, common::MAINNET_UTXO_HEIGHTS, common::MAINNET_BLOCK_HEIGHT, true)
        .unwrap();
    validator
        .verify_script(&tx, common::MAINNET_UTXO_HEIGHTS, common::MAINNET_BLOCK_HEIGHT, true)
        .unwrap();
    validator
        .verify_script_with_custom_flags(
            &tx,
            common::MAINNET_UTXO_HEIGHTS,
            common::MAINNET_BLOCK_HEIGHT,
            true,
            common::MAINNET_CUSTOM_FLAGS,
        )
        .unwrap();

    assert_eq!(
        validator
            .get_sig_op_count(
                &tx,
                common::MAINNET_UTXO_HEIGHTS,
                common::MAINNET_BLOCK_HEIGHT,
                true,
                false
            )
            .unwrap(),
        1
    );
}

#[test]
fn txvalidator_failing_paths_return_typed_errors() {
    let _guard = policy_guard();
    let tx = common::decode_hex(common::MAINNET_TX_HEX);
    let validator = validator("main");

    assert!(
        validator
            .validate_transaction(&tx, &[], common::MAINNET_BLOCK_HEIGHT, true)
            .is_err()
    );

    let malformed = validator
        .validate_transaction(&[0], &[0], 0, true)
        .unwrap_err();
    assert_eq!(malformed, TxError::Exception);
}

#[test]
fn validate_batch_matches_individual_validation() {
    let _guard = policy_guard();
    let tx = common::decode_hex(common::MAINNET_TX_HEX);
    let validator = validator("main");
    let mut batch = ValidateBatch::with_capacity(2);

    batch.add(&tx, common::MAINNET_UTXO_HEIGHTS, common::MAINNET_BLOCK_HEIGHT, true);
    batch.add(&tx, common::MAINNET_UTXO_HEIGHTS, common::MAINNET_BLOCK_HEIGHT, true);

    assert_eq!(batch.len(), 2);
    validator
        .validate_transaction(&tx, common::MAINNET_UTXO_HEIGHTS, common::MAINNET_BLOCK_HEIGHT, true)
        .unwrap();
    assert!(validator.validate_batch(&batch).into_iter().all(|r| r.is_ok()));

    batch.clear();
    assert!(batch.is_empty());
}

#[test]
fn script_error_mapping_and_count_match_core() {
    // This guard matches the Go test's count-only parity check. It catches
    // added/removed variants, but not a same-count enum reordering.
    assert_eq!(ScriptError::COUNT, ScriptError::cpp_error_count());

    for code in 0..ScriptError::COUNT {
        let err = ScriptError::from_code(code);
        assert_eq!(err.code(), code);
        assert!(!err.message().is_empty(), "empty message for code {code}");
    }

    assert_eq!(
        ScriptError::from_code(ScriptError::COUNT),
        ScriptError::Unknown(ScriptError::COUNT)
    );
    assert_eq!(TxError::Exception.message(), "C++ exception");
}

#[test]
fn standardness_policy_matches_go_behaviour() {
    let _guard = policy_guard();
    let tx = common::decode_hex(common::STANDARDNESS_TX_HEX);
    let utxo_heights = [945_789];
    let block_height = 945_789;

    validator("main")
        .validate_transaction(&tx, &utxo_heights, block_height, false)
        .unwrap();
    validator("main")
        .validate_transaction(&tx, &utxo_heights, block_height, true)
        .unwrap();

    let relaxed = validator("main");
    relaxed.set_require_standard(false);
    relaxed
        .validate_transaction(&tx, &utxo_heights, block_height, false)
        .unwrap();

    let accepts_nonstandard = validator("main");
    accepts_nonstandard.set_require_standard(true);
    accepts_nonstandard.set_accept_non_standard_output(true);
    accepts_nonstandard
        .validate_transaction(&tx, &utxo_heights, block_height, false)
        .unwrap();

    let rejects_nonstandard = validator("main");
    rejects_nonstandard.set_require_standard(true);
    rejects_nonstandard.set_accept_non_standard_output(false);
    assert!(
        rejects_nonstandard
            .validate_transaction(&tx, &utxo_heights, block_height, false)
            .is_err()
    );

    rejects_nonstandard.reset_default();
    restore_mainnet_activation_heights(&rejects_nonstandard);
}

#[test]
fn version_values_match_go_version_tests() {
    assert_eq!(bsv_client_version_major(), 1);
    assert_eq!(bsv_client_version_minor(), 2);
    assert_eq!(bsv_client_version_revision(), 2);
    assert_eq!(bdk_version_major(), 1);
    assert_eq!(bdk_version_minor(), 2);
    assert_eq!(bdk_version_patch(), 2);
    let rust_version_parts = [
        bdk_rust_version_major(),
        bdk_rust_version_minor(),
        bdk_rust_version_patch(),
    ];
    assert!(rust_version_parts.iter().all(|part| *part >= 0));

    assert!(!bsv_version_string().is_empty());
    assert!(!bdk_version_string().is_empty());
    assert!(!bdk_rust_version_string().is_empty());
}

fn validator(network: &str) -> TxValidator {
    TxValidator::new(network).unwrap_or_else(|| panic!("failed to create validator for {network}"))
}

fn policy_guard() -> MutexGuard<'static, ()> {
    POLICY_LOCK.lock().unwrap_or_else(|err| err.into_inner())
}

fn restore_mainnet_activation_heights(validator: &TxValidator) {
    validator.set_genesis_activation_height(620_538).unwrap();
    validator
        .set_chronicle_activation_height(943_816)
        .unwrap();
}
