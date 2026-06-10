#include <bdkffi/txvalidator.h>

#include <core/txvalidator.hpp>
#include <core/validatearg.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bdkffi_TxError ffi_exception()
{
    return {BDKFFI_TX_ERR_DOMAIN_EXCEPTION, 0};
}

bdkffi_TxError to_ffi_error(const TxError& error)
{
    return {error.domain, error.code};
}

void log_exception(const char* function_name, const char* message)
{
    try {
        std::cerr << "bdkffi exception in " << function_name << ": " << message << std::endl;
    } catch (...) {
    }
}

char* malloc_c_string(const std::string& value)
{
    if (value.empty()) {
        return nullptr;
    }

    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (out == nullptr) {
        return nullptr;
    }
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

char* malloc_exception_string(const char* message)
{
    return malloc_c_string(message == nullptr ? "unknown C++ exception" : message);
}

bsv::CTxValidator* checked_validator(bdkffi_txvalidator_t validator)
{
    if (validator == nullptr) {
        throw std::invalid_argument("null txvalidator");
    }
    return static_cast<bsv::CTxValidator*>(validator);
}

bsv::ValidateBatch* checked_batch(bdkffi_validatebatch_t batch)
{
    if (batch == nullptr) {
        throw std::invalid_argument("null validate batch");
    }
    return static_cast<bsv::ValidateBatch*>(batch);
}

std::span<const uint8_t> byte_span(const char* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative transaction length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null transaction pointer with positive length");
    }
    return {reinterpret_cast<const uint8_t*>(ptr), static_cast<size_t>(len)};
}

std::span<const int32_t> int32_span(const int32_t* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative utxo height length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null utxo height pointer with positive length");
    }
    return {ptr, static_cast<size_t>(len)};
}

std::span<const uint32_t> uint32_span(const uint32_t* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative custom flags length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null custom flags pointer with positive length");
    }
    return {ptr, static_cast<size_t>(len)};
}

bdkffi_TxError* malloc_error_array(const std::vector<TxError>& results, int* result_size)
{
    if (result_size != nullptr) {
        *result_size = static_cast<int>(results.size());
    }
    if (results.empty()) {
        return nullptr;
    }

    bdkffi_TxError* out = static_cast<bdkffi_TxError*>(
        std::malloc(results.size() * sizeof(bdkffi_TxError)));
    if (out == nullptr) {
        if (result_size != nullptr) {
            *result_size = 0;
        }
        return nullptr;
    }

    for (size_t i = 0; i < results.size(); ++i) {
        out[i] = to_ffi_error(results[i]);
    }
    return out;
}

bdkffi_TxError* malloc_single_exception(int* result_size)
{
    if (result_size == nullptr) {
        return nullptr;
    }

    bdkffi_TxError* out = static_cast<bdkffi_TxError*>(std::malloc(sizeof(bdkffi_TxError)));
    if (out == nullptr) {
        *result_size = 0;
        return nullptr;
    }

    *result_size = 1;
    out[0] = ffi_exception();
    return out;
}

} // namespace

#define BDKFFI_CATCH_RETURN(value) \
    catch (const std::exception& e) { \
        log_exception(__func__, e.what()); \
        return (value); \
    } \
    catch (...) { \
        log_exception(__func__, "unknown C++ exception"); \
        return (value); \
    }

#define BDKFFI_CATCH_VOID \
    catch (const std::exception& e) { \
        log_exception(__func__, e.what()); \
    } \
    catch (...) { \
        log_exception(__func__, "unknown C++ exception"); \
    }

#define BDKFFI_SETTER_1(function_name, method_name, value_type) \
    extern "C" char* function_name(bdkffi_txvalidator_t validator, value_type value) \
    { \
        try { \
            std::string error; \
            const bool ok = checked_validator(validator)->method_name(value, &error); \
            if (!ok && error.empty()) { \
                error = "setter failed"; \
            } \
            return malloc_c_string(error); \
        } catch (const std::exception& e) { \
            return malloc_exception_string(e.what()); \
        } catch (...) { \
            return malloc_exception_string("unknown C++ exception"); \
        } \
    }

#define BDKFFI_VOID_SETTER_1(function_name, method_name, value_type) \
    extern "C" void function_name(bdkffi_txvalidator_t validator, value_type value) \
    { \
        try { \
            checked_validator(validator)->method_name(value); \
        } BDKFFI_CATCH_VOID \
    }

extern "C" bdkffi_txvalidator_t bdkffi_txvalidator_create(const char* network_name, int network_name_len)
{
    try {
        if (network_name_len < 0) {
            throw std::invalid_argument("negative network name length");
        }
        if (network_name_len > 0 && network_name == nullptr) {
            throw std::invalid_argument("null network name pointer with positive length");
        }
        const std::string network(network_name == nullptr ? "" : network_name, static_cast<size_t>(network_name_len));
        return new bsv::CTxValidator(network);
    } BDKFFI_CATCH_RETURN(nullptr)
}

extern "C" void bdkffi_txvalidator_destroy(bdkffi_txvalidator_t validator)
{
    try {
        delete static_cast<bsv::CTxValidator*>(validator);
    } BDKFFI_CATCH_VOID
}

BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_ops_per_script_policy, SetMaxOpsPerScriptPolicy, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_script_num_length_policy, SetMaxScriptNumLengthPolicy, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_script_size_policy, SetMaxScriptSizePolicy, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_pub_keys_per_multisig_policy, SetMaxPubKeysPerMultiSigPolicy, int64_t)

extern "C" char* bdkffi_txvalidator_set_max_stack_memory_usage(
    bdkffi_txvalidator_t validator,
    int64_t consensus_value,
    int64_t policy_value)
{
    try {
        std::string error;
        const bool ok = checked_validator(validator)->SetMaxStackMemoryUsage(consensus_value, policy_value, &error);
        if (!ok && error.empty()) {
            error = "setter failed";
        }
        return malloc_c_string(error);
    } catch (const std::exception& e) {
        return malloc_exception_string(e.what());
    } catch (...) {
        return malloc_exception_string("unknown C++ exception");
    }
}

BDKFFI_SETTER_1(bdkffi_txvalidator_set_genesis_activation_height, SetGenesisActivationHeight, int32_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_chronicle_activation_height, SetChronicleActivationHeight, int32_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_genesis_graceful_period, SetGenesisGracefulPeriod, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_chronicle_graceful_period, SetChronicleGracefulPeriod, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_tx_size_policy, SetMaxTxSizePolicy, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_sig_ops_post_genesis_policy, SetMaxSigOpsPostGenesisPolicy, int64_t)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_max_sig_ops_policy, SetMaxSigOpsPolicy, uint64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_min_consolidation_factor, SetMinConsolidationFactor, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_max_consolidation_input_script_size, SetMaxConsolidationInputScriptSize, int64_t)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_min_conf_consolidation_input, SetMinConfConsolidationInput, int64_t)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_accept_non_std_consolidation_input, SetAcceptNonStdConsolidationInput, bool)
BDKFFI_SETTER_1(bdkffi_txvalidator_set_min_mining_tx_fee, SetMinMiningTxFee, int64_t)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_data_carrier_size, SetDataCarrierSize, uint64_t)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_data_carrier, SetDataCarrier, bool)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_accept_non_standard_output, SetAcceptNonStandardOutput, bool)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_require_standard, SetRequireStandard, bool)
BDKFFI_VOID_SETTER_1(bdkffi_txvalidator_set_permit_bare_multisig, SetPermitBareMultisig, bool)

extern "C" void bdkffi_txvalidator_reset_default(bdkffi_txvalidator_t validator)
{
    try {
        checked_validator(validator)->ResetDefault();
    } BDKFFI_CATCH_VOID
}

extern "C" uint64_t bdkffi_txvalidator_get_max_ops_per_script(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxOpsPerScript(is_genesis_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_script_num_length(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_chronicle_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxScriptNumLength(is_genesis_enabled, is_chronicle_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_script_size(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxScriptSize(is_genesis_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_pub_keys_per_multisig(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxPubKeysPerMultiSig(is_genesis_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_stack_memory_usage(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxStackMemoryUsage(is_genesis_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_tx_size(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_chronicle_enabled,
    bool is_consensus)
{
    try {
        return checked_validator(validator)->GetMaxTxSize(is_genesis_enabled, is_chronicle_enabled, is_consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_data_carrier_size(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetDataCarrierSize();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" bool bdkffi_txvalidator_get_data_carrier(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetDataCarrier();
    } BDKFFI_CATCH_RETURN(false)
}

extern "C" bool bdkffi_txvalidator_get_accept_non_standard_output(
    bdkffi_txvalidator_t validator,
    bool is_genesis_enabled,
    bool is_chronicle_enabled)
{
    try {
        return checked_validator(validator)->GetAcceptNonStandardOutput(is_genesis_enabled, is_chronicle_enabled);
    } BDKFFI_CATCH_RETURN(false)
}

extern "C" bool bdkffi_txvalidator_get_require_standard(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetRequireStandard();
    } BDKFFI_CATCH_RETURN(false)
}

extern "C" bool bdkffi_txvalidator_get_permit_bare_multisig(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetPermitBareMultisig();
    } BDKFFI_CATCH_RETURN(false)
}

extern "C" int32_t bdkffi_txvalidator_get_genesis_activation_height(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetGenesisActivationHeight();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" int32_t bdkffi_txvalidator_get_chronicle_activation_height(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetChronicleActivationHeight();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_genesis_graceful_period(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetGenesisGracefulPeriod();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_chronicle_graceful_period(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetChronicleGracefulPeriod();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_min_consolidation_factor(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMinConsolidationFactor();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_consolidation_input_script_size(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMaxConsolidationInputScriptSize();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_min_conf_consolidation_input(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMinConfConsolidationInput();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" bool bdkffi_txvalidator_get_accept_non_std_consolidation_input(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetAcceptNonStdConsolidationInput();
    } BDKFFI_CATCH_RETURN(false)
}

extern "C" int64_t bdkffi_txvalidator_get_min_mining_tx_fee(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMinMiningTxFee();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_sig_ops_policy(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMaxSigOpsPolicy();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_max_sig_ops_post_genesis_policy(bdkffi_txvalidator_t validator)
{
    try {
        return checked_validator(validator)->GetMaxSigOpsPostGenesisPolicy();
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" uint64_t bdkffi_txvalidator_get_sig_op_count(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool count_p2sh_sig_ops,
    bool consensus,
    char** err_str)
{
    try {
        if (err_str != nullptr) {
            *err_str = nullptr;
        }
        return checked_validator(validator)->GetSigOpCount(
            byte_span(extended_tx, extended_tx_len),
            int32_span(utxo_heights, utxo_heights_len),
            block_height,
            count_p2sh_sig_ops,
            consensus);
    } catch (const std::exception& e) {
        if (err_str != nullptr) {
            *err_str = malloc_exception_string(e.what());
        }
        return 0;
    } catch (...) {
        if (err_str != nullptr) {
            *err_str = malloc_exception_string("unknown C++ exception");
        }
        return 0;
    }
}

extern "C" uint32_t bdkffi_txvalidator_calculate_flags(
    bdkffi_txvalidator_t validator,
    int32_t utxo_height,
    int32_t block_height,
    bool consensus)
{
    try {
        return checked_validator(validator)->CalculateFlags(utxo_height, block_height, consensus);
    } BDKFFI_CATCH_RETURN(0)
}

extern "C" bdkffi_TxError bdkffi_txvalidator_verify_script(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus)
{
    try {
        return to_ffi_error(checked_validator(validator)->VerifyScript(
            byte_span(extended_tx, extended_tx_len),
            int32_span(utxo_heights, utxo_heights_len),
            block_height,
            consensus));
    } BDKFFI_CATCH_RETURN(ffi_exception())
}

extern "C" bdkffi_TxError bdkffi_txvalidator_verify_script_with_custom_flags(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus,
    const uint32_t* custom_flags,
    int custom_flags_len)
{
    try {
        return to_ffi_error(checked_validator(validator)->VerifyScript(
            byte_span(extended_tx, extended_tx_len),
            int32_span(utxo_heights, utxo_heights_len),
            block_height,
            consensus,
            uint32_span(custom_flags, custom_flags_len)));
    } BDKFFI_CATCH_RETURN(ffi_exception())
}

extern "C" bdkffi_TxError bdkffi_txvalidator_validate_transaction(
    bdkffi_txvalidator_t validator,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus)
{
    try {
        return to_ffi_error(checked_validator(validator)->ValidateTransaction(
            byte_span(extended_tx, extended_tx_len),
            int32_span(utxo_heights, utxo_heights_len),
            block_height,
            consensus));
    } BDKFFI_CATCH_RETURN(ffi_exception())
}

extern "C" bdkffi_TxError* bdkffi_txvalidator_validate_batch(
    bdkffi_txvalidator_t validator,
    bdkffi_validatebatch_t batch,
    int* result_size)
{
    try {
        if (result_size != nullptr) {
            *result_size = 0;
        }
        return malloc_error_array(checked_validator(validator)->ValidateBatch(*checked_batch(batch)), result_size);
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        return malloc_single_exception(result_size);
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        return malloc_single_exception(result_size);
    }
}

#undef BDKFFI_SETTER_1
#undef BDKFFI_VOID_SETTER_1
#undef BDKFFI_CATCH_RETURN
#undef BDKFFI_CATCH_VOID
