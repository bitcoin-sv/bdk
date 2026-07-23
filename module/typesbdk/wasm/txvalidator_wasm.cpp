#include "txvalidator_wasm.h"
#include <txvalidator.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

extern "C" void bdk_secp256k1_prepare_verification_tables(void);

namespace {

enum class WasmNetwork : uint32_t {
    Main = 0,
    Test = 1,
    Stn = 2,
    Regtest = 3,
    TeraTestnet = 4,
    TeraScalingTestnet = 5
};

const bsv::CTxValidator& MainValidator() noexcept
{
    static const bsv::CTxValidator validator{bsv::TxValidationNetwork::Main};
    return validator;
}

const bsv::CTxValidator* ValidatorForNetwork(uint32_t network) noexcept
{
    switch(static_cast<WasmNetwork>(network)) {
    case WasmNetwork::Main:
        return &MainValidator();
    case WasmNetwork::Test: {
        static const bsv::CTxValidator validator{bsv::TxValidationNetwork::Test};
        return &validator;
    }
    case WasmNetwork::Stn: {
        static const bsv::CTxValidator validator{bsv::TxValidationNetwork::Stn};
        return &validator;
    }
    case WasmNetwork::Regtest: {
        static const bsv::CTxValidator validator{bsv::TxValidationNetwork::Regtest};
        return &validator;
    }
    case WasmNetwork::TeraTestnet: {
        static const bsv::CTxValidator validator{bsv::TxValidationNetwork::TeraTestnet};
        return &validator;
    }
    case WasmNetwork::TeraScalingTestnet: {
        static const bsv::CTxValidator validator{bsv::TxValidationNetwork::TeraScalingTestnet};
        return &validator;
    }
    }
    return nullptr;
}

template<typename T>
std::span<const T> Span(const T* data, uint32_t size) noexcept
{
    return data == nullptr ? std::span<const T>{} : std::span<const T>{data, size};
}

template<typename T>
bool ValidOffsets(const T* offsets, uint32_t entryCount, uint32_t valueCount) noexcept
{
    if(offsets == nullptr || offsets[0] != 0 || offsets[entryCount] != valueCount)
        return false;
    for(uint32_t index = 0; index < entryCount; ++index) {
        if(offsets[index] > offsets[index + 1])
            return false;
    }
    return true;
}

template<typename T>
std::span<const T> Slice(const T* values, const uint32_t* offsets, uint32_t index) noexcept
{
    const uint32_t size = offsets[index + 1] - offsets[index];
    return size == 0
        ? std::span<const T>{}
        : std::span<const T>{values + offsets[index], size};
}

void WriteResult(int32_t* output, uint32_t index, TxError result) noexcept
{
    output[index * 2] = result.domain;
    output[index * 2 + 1] = result.code;
}

TxError ExceptionResult() noexcept
{
    return {TX_ERR_DOMAIN_EXCEPTION, 0};
}

} // namespace

extern "C" {

void bdk_verify_script_main(
    const uint8_t* extendedTX,
    uint32_t extendedTXSize,
    const int32_t* utxoHeights,
    uint32_t utxoHeightCount,
    int32_t blockHeight,
    uint32_t consensus,
    const uint32_t* customFlags,
    uint32_t customFlagCount,
    int32_t* output) noexcept
{
    if(output == nullptr)
        return;
    bdk_secp256k1_prepare_verification_tables();
    WriteResult(output, 0, MainValidator().VerifyScript(
        Span(extendedTX, extendedTXSize),
        Span(utxoHeights, utxoHeightCount),
        blockHeight,
        consensus != 0,
        Span(customFlags, customFlagCount)));
}

void bdk_verify_script(
    const uint8_t* extendedTX,
    uint32_t extendedTXSize,
    const int32_t* utxoHeights,
    uint32_t utxoHeightCount,
    int32_t blockHeight,
    uint32_t consensus,
    const uint32_t* customFlags,
    uint32_t customFlagCount,
    uint32_t network,
    int32_t* output) noexcept
{
    if(output == nullptr)
        return;
    const bsv::CTxValidator* validator = ValidatorForNetwork(network);
    TxError result = ExceptionResult();
    if(validator != nullptr) {
        bdk_secp256k1_prepare_verification_tables();
        result = validator->VerifyScript(
            Span(extendedTX, extendedTXSize),
            Span(utxoHeights, utxoHeightCount),
            blockHeight,
            consensus != 0,
            Span(customFlags, customFlagCount));
    }
    WriteResult(output, 0, result);
}

void bdk_verify_script_batch(
    const uint8_t* extendedTXs,
    uint32_t extendedTXSize,
    const uint32_t* txOffsets,
    const int32_t* utxoHeights,
    uint32_t utxoHeightCount,
    const uint32_t* heightOffsets,
    const int32_t* blockHeights,
    const uint8_t* consensus,
    const uint32_t* customFlags,
    uint32_t customFlagCount,
    const uint32_t* customFlagOffsets,
    uint32_t entryCount,
    uint32_t network,
    int32_t* output) noexcept
{
    if(output == nullptr)
        return;
    const bsv::CTxValidator* validator = ValidatorForNetwork(network);
    if(validator == nullptr || blockHeights == nullptr || consensus == nullptr ||
       (extendedTXSize != 0 && extendedTXs == nullptr) ||
       (utxoHeightCount != 0 && utxoHeights == nullptr) ||
       (customFlagCount != 0 && customFlags == nullptr) ||
       !ValidOffsets(txOffsets, entryCount, extendedTXSize) ||
       !ValidOffsets(heightOffsets, entryCount, utxoHeightCount) ||
       !ValidOffsets(customFlagOffsets, entryCount, customFlagCount)) {
        for(uint32_t index = 0; index < entryCount; ++index)
            WriteResult(output, index, ExceptionResult());
        return;
    }

    bdk_secp256k1_prepare_verification_tables();
    for(uint32_t index = 0; index < entryCount; ++index) {
        WriteResult(output, index, validator->VerifyScript(
            Slice(extendedTXs, txOffsets, index),
            Slice(utxoHeights, heightOffsets, index),
            blockHeights[index],
            consensus[index] != 0,
            Slice(customFlags, customFlagOffsets, index)));
    }
}

void bdk_verify_spend(
    const uint8_t* transaction,
    uint32_t transactionSize,
    uint32_t inputIndex,
    const uint8_t* lockingScript,
    uint32_t lockingScriptSize,
    double sourceSatoshis,
    int32_t utxoHeight,
    int32_t blockHeight,
    uint32_t consensus,
    uint32_t hasCustomFlags,
    uint32_t customFlags,
    uint32_t network,
    int32_t* output) noexcept
{
    if(output == nullptr)
        return;
    const bsv::CTxValidator* validator = ValidatorForNetwork(network);
    TxError result = ExceptionResult();
    if(validator != nullptr && std::isfinite(sourceSatoshis) &&
       std::floor(sourceSatoshis) == sourceSatoshis && sourceSatoshis >= 0 &&
       sourceSatoshis <= 9007199254740991.0) {
        bdk_secp256k1_prepare_verification_tables();
        result = validator->VerifySpend(
            Span(transaction, transactionSize),
            inputIndex,
            Span(lockingScript, lockingScriptSize),
            static_cast<int64_t>(sourceSatoshis),
            utxoHeight,
            blockHeight,
            consensus != 0,
            hasCustomFlags != 0 ? std::optional<uint32_t>{customFlags} : std::nullopt);
    }
    WriteResult(output, 0, result);
}

void bdk_verify_spend_batch(
    const uint8_t* transactions,
    uint32_t transactionSize,
    const uint32_t* transactionOffsets,
    const uint32_t* inputIndices,
    const uint8_t* lockingScripts,
    uint32_t lockingScriptSize,
    const uint32_t* lockingScriptOffsets,
    const double* sourceSatoshis,
    const int32_t* utxoHeights,
    const int32_t* blockHeights,
    const uint8_t* consensus,
    const uint8_t* hasCustomFlags,
    const uint32_t* customFlags,
    uint32_t entryCount,
    uint32_t network,
    int32_t* output) noexcept
{
    if(output == nullptr)
        return;
    const bsv::CTxValidator* validator = ValidatorForNetwork(network);
    if(validator == nullptr || inputIndices == nullptr || sourceSatoshis == nullptr ||
       utxoHeights == nullptr || blockHeights == nullptr || consensus == nullptr ||
       hasCustomFlags == nullptr || customFlags == nullptr ||
       (transactionSize != 0 && transactions == nullptr) ||
       (lockingScriptSize != 0 && lockingScripts == nullptr) ||
       !ValidOffsets(transactionOffsets, entryCount, transactionSize) ||
       !ValidOffsets(lockingScriptOffsets, entryCount, lockingScriptSize)) {
        for(uint32_t index = 0; index < entryCount; ++index)
            WriteResult(output, index, ExceptionResult());
        return;
    }

    bdk_secp256k1_prepare_verification_tables();
    for(uint32_t index = 0; index < entryCount; ++index) {
        TxError result = ExceptionResult();
        if(std::isfinite(sourceSatoshis[index]) &&
           std::floor(sourceSatoshis[index]) == sourceSatoshis[index] &&
           sourceSatoshis[index] >= 0 && sourceSatoshis[index] <= 9007199254740991.0) {
            result = validator->VerifySpend(
                Slice(transactions, transactionOffsets, index),
                inputIndices[index],
                Slice(lockingScripts, lockingScriptOffsets, index),
                static_cast<int64_t>(sourceSatoshis[index]),
                utxoHeights[index],
                blockHeights[index],
                consensus[index] != 0,
                hasCustomFlags[index] != 0
                    ? std::optional<uint32_t>{customFlags[index]}
                    : std::nullopt);
        }
        WriteResult(output, index, result);
    }
}

} // extern "C"
