#ifndef BDKFFI_TXVALIDATOR_H
#define BDKFFI_TXVALIDATOR_H

#include <stdbool.h>
#include <stdint.h>

#include <bdkffi/txerror.h>
#include <bdkffi/validatebatch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* bdkffi_txvalidator_t;

bdkffi_txvalidator_t bdkffi_txvalidator_create(const char* network_name, int network_name_len);
void bdkffi_txvalidator_destroy(bdkffi_txvalidator_t validator);

/*
 * Fallible setters return NULL on success, otherwise a malloc-owned
 * null-terminated error string. Free it with bdkffi_free.
 */
char* bdkffi_txvalidator_set_max_ops_per_script_policy(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_script_num_length_policy(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_script_size_policy(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_pub_keys_per_multisig_policy(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_stack_memory_usage(bdkffi_txvalidator_t validator, int64_t consensus_value, int64_t policy_value);
char* bdkffi_txvalidator_set_genesis_activation_height(bdkffi_txvalidator_t validator, int32_t value);
char* bdkffi_txvalidator_set_chronicle_activation_height(bdkffi_txvalidator_t validator, int32_t value);
char* bdkffi_txvalidator_set_genesis_graceful_period(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_chronicle_graceful_period(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_tx_size_policy(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_sig_ops_post_genesis_policy(bdkffi_txvalidator_t validator, int64_t value);
void bdkffi_txvalidator_set_max_sig_ops_policy(bdkffi_txvalidator_t validator, uint64_t value);
char* bdkffi_txvalidator_set_min_consolidation_factor(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_max_consolidation_input_script_size(bdkffi_txvalidator_t validator, int64_t value);
char* bdkffi_txvalidator_set_min_conf_consolidation_input(bdkffi_txvalidator_t validator, int64_t value);
void bdkffi_txvalidator_set_accept_non_std_consolidation_input(bdkffi_txvalidator_t validator, bool value);
char* bdkffi_txvalidator_set_min_mining_tx_fee(bdkffi_txvalidator_t validator, int64_t satoshis_per_kb);
void bdkffi_txvalidator_set_data_carrier_size(bdkffi_txvalidator_t validator, uint64_t value);
void bdkffi_txvalidator_set_data_carrier(bdkffi_txvalidator_t validator, bool value);
void bdkffi_txvalidator_set_accept_non_standard_output(bdkffi_txvalidator_t validator, bool value);
void bdkffi_txvalidator_set_require_standard(bdkffi_txvalidator_t validator, bool value);
void bdkffi_txvalidator_set_permit_bare_multisig(bdkffi_txvalidator_t validator, bool value);
void bdkffi_txvalidator_reset_default(bdkffi_txvalidator_t validator);

uint64_t bdkffi_txvalidator_get_max_ops_per_script(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_max_script_num_length(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_chronicle_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_max_script_size(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_max_pub_keys_per_multisig(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_max_stack_memory_usage(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_max_tx_size(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_chronicle_enabled, bool is_consensus);
uint64_t bdkffi_txvalidator_get_data_carrier_size(bdkffi_txvalidator_t validator);
bool bdkffi_txvalidator_get_data_carrier(bdkffi_txvalidator_t validator);
bool bdkffi_txvalidator_get_accept_non_standard_output(bdkffi_txvalidator_t validator, bool is_genesis_enabled, bool is_chronicle_enabled);
bool bdkffi_txvalidator_get_require_standard(bdkffi_txvalidator_t validator);
bool bdkffi_txvalidator_get_permit_bare_multisig(bdkffi_txvalidator_t validator);
int32_t bdkffi_txvalidator_get_genesis_activation_height(bdkffi_txvalidator_t validator);
int32_t bdkffi_txvalidator_get_chronicle_activation_height(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_genesis_graceful_period(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_chronicle_graceful_period(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_min_consolidation_factor(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_max_consolidation_input_script_size(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_min_conf_consolidation_input(bdkffi_txvalidator_t validator);
bool bdkffi_txvalidator_get_accept_non_std_consolidation_input(bdkffi_txvalidator_t validator);
int64_t bdkffi_txvalidator_get_min_mining_tx_fee(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_max_sig_ops_policy(bdkffi_txvalidator_t validator);
uint64_t bdkffi_txvalidator_get_max_sig_ops_post_genesis_policy(bdkffi_txvalidator_t validator);

/*
 * On success, err_str is set to NULL and the return value is the sigop count.
 * On failure, the return value is 0 and err_str receives a malloc-owned,
 * null-terminated error string when err_str is non-NULL. Free it with
 * bdkffi_free.
 */
uint64_t bdkffi_txvalidator_get_sig_op_count(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool count_p2sh_sig_ops,
    bool consensus,
    char** err_str);

uint32_t bdkffi_txvalidator_calculate_flags(
    bdkffi_txvalidator_t validator,
    int32_t utxo_height,
    int32_t block_height,
    bool consensus);

bdkffi_TxError bdkffi_txvalidator_verify_script(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus);

bdkffi_TxError bdkffi_txvalidator_verify_script_with_custom_flags(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus,
    const uint32_t* custom_flags,
    int custom_flags_len);

bdkffi_TxError bdkffi_txvalidator_validate_transaction(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus);

/*
 * Returns a malloc-owned array of bdkffi_TxError values and writes its length
 * to result_size. Free the returned pointer with bdkffi_free.
 *
 * Normal success and per-transaction validation failures preserve positional
 * mapping with the batch size. If the shim itself catches an internal exception
 * before core can return the vector, result_size is 1 and the single element is
 * {BDKFFI_TX_ERR_DOMAIN_EXCEPTION, 0}; safe wrappers should treat a length
 * mismatch as all entries failing with EXCEPTION.
 */
bdkffi_TxError* bdkffi_txvalidator_validate_batch(
    bdkffi_txvalidator_t validator,
    bdkffi_validatebatch_t batch,
    int* result_size);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_TXVALIDATOR_H */
