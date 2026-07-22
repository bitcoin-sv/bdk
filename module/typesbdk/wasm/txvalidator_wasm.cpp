#include "txvalidator_wasm.h"
#include <txvalidator.hpp>

#include <emscripten/bind.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;

enum class WasmNetwork : uint32_t {
    Main = 0,
    Test = 1,
    Stn = 2,
    Regtest = 3,
    TeraTestnet = 4,
    TeraScalingTestnet = 5
};

template<typename T>
std::vector<T> CopyNumberArray(const emscripten::val& values)
{
    const auto size = values["length"].as<size_t>();
    std::vector<T> result(size);
    if(size != 0) {
        emscripten::val view{emscripten::typed_memory_view(size, result.data())};
        view.call<void>("set", values);
    }
    return result;
}

const bsv::CTxValidator& ValidatorForNetwork(uint32_t network)
{
    switch(static_cast<WasmNetwork>(network)) {
    case WasmNetwork::Main: {
        static const bsv::CTxValidator validator{"main"};
        return validator;
    }
    case WasmNetwork::Test: {
        static const bsv::CTxValidator validator{"test"};
        return validator;
    }
    case WasmNetwork::Stn: {
        static const bsv::CTxValidator validator{"stn"};
        return validator;
    }
    case WasmNetwork::Regtest: {
        static const bsv::CTxValidator validator{"regtest"};
        return validator;
    }
    case WasmNetwork::TeraTestnet: {
        static const bsv::CTxValidator validator{"teratestnet"};
        return validator;
    }
    case WasmNetwork::TeraScalingTestnet: {
        static const bsv::CTxValidator validator{"tstn"};
        return validator;
    }
    }
    throw std::invalid_argument("unknown BDK network");
}

template<typename T>
void ValidateOffsets(
    const std::vector<T>& values,
    const std::vector<uint32_t>& offsets,
    size_t entryCount,
    const char* name)
{
    if(offsets.size() != entryCount + 1 || offsets.front() != 0 || offsets.back() != values.size())
        throw std::invalid_argument(std::string{"invalid "} + name + " offsets");
    for(size_t index = 0; index < entryCount; ++index) {
        if(offsets[index] > offsets[index + 1])
            throw std::invalid_argument(std::string{"non-monotonic "} + name + " offsets");
    }
}

template<typename T>
std::span<const T> CheckedSlice(
    const std::vector<T>& values,
    const std::vector<uint32_t>& offsets,
    size_t index,
    size_t entryCount,
    const char* name)
{
    return std::span<const T>{values}.subspan(offsets[index], offsets[index + 1] - offsets[index]);
}

emscripten::val ToInt32Array(const std::vector<int32_t>& values)
{
    emscripten::val result = emscripten::val::global("Int32Array").new_(values.size());
    if(!values.empty()) {
        emscripten::val view{emscripten::typed_memory_view(values.size(), values.data())};
        result.call<void>("set", view);
    }
    return result;
}

TxError VerifyScriptForNetwork(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags,
    uint32_t network)
{
    return ValidatorForNetwork(network).VerifyScript(
        std::span<const uint8_t>{extendedTX},
        std::span<const int32_t>{utxoHeights},
        blockHeight,
        consensus,
        std::span<const uint32_t>{customFlags});
}

TxError VerifyScriptArrayForNetwork(
    const emscripten::val& extendedTX,
    const emscripten::val& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const emscripten::val& customFlags,
    uint32_t network)
{
    return VerifyScriptForNetwork(
        CopyNumberArray<uint8_t>(extendedTX),
        CopyNumberArray<int32_t>(utxoHeights),
        blockHeight,
        consensus,
        CopyNumberArray<uint32_t>(customFlags),
        network);
}

emscripten::val VerifyScriptBatchArrayWASM(
    const emscripten::val& extendedTXsValue,
    const emscripten::val& txOffsetsValue,
    const emscripten::val& utxoHeightsValue,
    const emscripten::val& heightOffsetsValue,
    const emscripten::val& blockHeightsValue,
    const emscripten::val& consensusValue,
    const emscripten::val& customFlagsValue,
    const emscripten::val& customFlagOffsetsValue,
    uint32_t network)
{
    const auto extendedTXs = CopyNumberArray<uint8_t>(extendedTXsValue);
    const auto txOffsets = CopyNumberArray<uint32_t>(txOffsetsValue);
    const auto utxoHeights = CopyNumberArray<int32_t>(utxoHeightsValue);
    const auto heightOffsets = CopyNumberArray<uint32_t>(heightOffsetsValue);
    const auto blockHeights = CopyNumberArray<int32_t>(blockHeightsValue);
    const auto consensus = CopyNumberArray<uint8_t>(consensusValue);
    const auto customFlags = CopyNumberArray<uint32_t>(customFlagsValue);
    const auto customFlagOffsets = CopyNumberArray<uint32_t>(customFlagOffsetsValue);
    const size_t entryCount = blockHeights.size();

    if(consensus.size() != entryCount)
        throw std::invalid_argument("batch consensus length does not match block heights");
    ValidateOffsets(extendedTXs, txOffsets, entryCount, "transaction");
    ValidateOffsets(utxoHeights, heightOffsets, entryCount, "height");
    ValidateOffsets(customFlags, customFlagOffsets, entryCount, "custom flag");

    std::vector<int32_t> flattenedResults;
    flattenedResults.reserve(entryCount * 2);
    const auto& validator = ValidatorForNetwork(network);
    for(size_t index = 0; index < entryCount; ++index) {
        const auto tx = CheckedSlice(extendedTXs, txOffsets, index, entryCount, "transaction");
        const auto heights = CheckedSlice(utxoHeights, heightOffsets, index, entryCount, "height");
        const auto flags = CheckedSlice(customFlags, customFlagOffsets, index, entryCount, "custom flag");
        const TxError result = validator.VerifyScript(
            tx, heights, blockHeights[index], consensus[index] != 0, flags);
        flattenedResults.push_back(result.domain);
        flattenedResults.push_back(result.code);
    }
    return ToInt32Array(flattenedResults);
}

TxError VerifySpendArrayWASM(
    const emscripten::val& transactionValue,
    uint32_t inputIndex,
    const emscripten::val& lockingScriptValue,
    double sourceSatoshis,
    int32_t utxoHeight,
    int32_t blockHeight,
    bool consensus,
    bool hasCustomFlags,
    uint32_t customFlags,
    uint32_t network)
{
    if(!std::isfinite(sourceSatoshis) || std::floor(sourceSatoshis) != sourceSatoshis ||
       sourceSatoshis < 0 || sourceSatoshis > MAX_SAFE_INTEGER)
        throw std::invalid_argument("source satoshis must be a non-negative safe integer");
    return ValidatorForNetwork(network).VerifySpend(
        CopyNumberArray<uint8_t>(transactionValue),
        inputIndex,
        CopyNumberArray<uint8_t>(lockingScriptValue),
        static_cast<int64_t>(sourceSatoshis),
        utxoHeight,
        blockHeight,
        consensus,
        hasCustomFlags ? std::optional<uint32_t>{customFlags} : std::nullopt);
}

emscripten::val VerifySpendBatchArrayWASM(
    const emscripten::val& transactionsValue,
    const emscripten::val& transactionOffsetsValue,
    const emscripten::val& inputIndicesValue,
    const emscripten::val& lockingScriptsValue,
    const emscripten::val& lockingScriptOffsetsValue,
    const emscripten::val& sourceSatoshisValue,
    const emscripten::val& utxoHeightsValue,
    const emscripten::val& blockHeightsValue,
    const emscripten::val& consensusValue,
    const emscripten::val& hasCustomFlagsValue,
    const emscripten::val& customFlagsValue,
    uint32_t network)
{
    const auto transactions = CopyNumberArray<uint8_t>(transactionsValue);
    const auto transactionOffsets = CopyNumberArray<uint32_t>(transactionOffsetsValue);
    const auto inputIndices = CopyNumberArray<uint32_t>(inputIndicesValue);
    const auto lockingScripts = CopyNumberArray<uint8_t>(lockingScriptsValue);
    const auto lockingScriptOffsets = CopyNumberArray<uint32_t>(lockingScriptOffsetsValue);
    const auto sourceSatoshis = CopyNumberArray<double>(sourceSatoshisValue);
    const auto utxoHeights = CopyNumberArray<int32_t>(utxoHeightsValue);
    const auto blockHeights = CopyNumberArray<int32_t>(blockHeightsValue);
    const auto consensus = CopyNumberArray<uint8_t>(consensusValue);
    const auto hasCustomFlags = CopyNumberArray<uint8_t>(hasCustomFlagsValue);
    const auto customFlags = CopyNumberArray<uint32_t>(customFlagsValue);
    const size_t entryCount = inputIndices.size();

    if(sourceSatoshis.size() != entryCount || utxoHeights.size() != entryCount ||
       blockHeights.size() != entryCount || consensus.size() != entryCount ||
       hasCustomFlags.size() != entryCount || customFlags.size() != entryCount)
        throw std::invalid_argument("spend batch metadata lengths do not match");
    ValidateOffsets(transactions, transactionOffsets, entryCount, "transaction");
    ValidateOffsets(lockingScripts, lockingScriptOffsets, entryCount, "locking script");

    std::vector<int32_t> flattenedResults;
    flattenedResults.reserve(entryCount * 2);
    const auto& validator = ValidatorForNetwork(network);
    for(size_t index = 0; index < entryCount; ++index) {
        if(!std::isfinite(sourceSatoshis[index]) || std::floor(sourceSatoshis[index]) != sourceSatoshis[index] ||
           sourceSatoshis[index] < 0 || sourceSatoshis[index] > MAX_SAFE_INTEGER) {
            flattenedResults.push_back(TX_ERR_DOMAIN_EXCEPTION);
            flattenedResults.push_back(0);
            continue;
        }
        const auto tx = CheckedSlice(transactions, transactionOffsets, index, entryCount, "transaction");
        const auto script = CheckedSlice(lockingScripts, lockingScriptOffsets, index, entryCount, "locking script");
        const TxError result = validator.VerifySpend(
            tx,
            inputIndices[index],
            script,
            static_cast<int64_t>(sourceSatoshis[index]),
            utxoHeights[index],
            blockHeights[index],
            consensus[index] != 0,
            hasCustomFlags[index] != 0 ? std::optional<uint32_t>{customFlags[index]} : std::nullopt);
        flattenedResults.push_back(result.domain);
        flattenedResults.push_back(result.code);
    }
    return ToInt32Array(flattenedResults);
}

} // namespace

TxError VerifyScriptWASM(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags)
{
    return VerifyScriptForNetwork(
        extendedTX, utxoHeights, blockHeight, consensus, customFlags,
        static_cast<uint32_t>(WasmNetwork::Main));
}

TxError VerifyScriptArrayWASM(
    const emscripten::val& extendedTX,
    const emscripten::val& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const emscripten::val& customFlags)
{
    return VerifyScriptArrayForNetwork(
        extendedTX, utxoHeights, blockHeight, consensus, customFlags,
        static_cast<uint32_t>(WasmNetwork::Main));
}

namespace esbind = emscripten;
EMSCRIPTEN_BINDINGS(bdk_module) {
    esbind::register_vector<uint8_t>("VectorUInt8");
    esbind::register_vector<int32_t>("VectorInt32");
    esbind::register_vector<uint32_t>("VectorUInt32");

    esbind::value_object<TxError>("TxError")
        .field("domain", &TxError::domain)
        .field("code", &TxError::code);

    esbind::function("VerifyScript", &VerifyScriptWASM);
    esbind::function("VerifyScriptArray", &VerifyScriptArrayWASM);
    esbind::function("VerifyScriptArrayNetwork", &VerifyScriptArrayForNetwork);
    esbind::function("VerifyScriptBatchArray", &VerifyScriptBatchArrayWASM);
    esbind::function("VerifySpendArray", &VerifySpendArrayWASM);
    esbind::function("VerifySpendBatchArray", &VerifySpendBatchArrayWASM);
}
