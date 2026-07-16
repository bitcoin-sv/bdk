#include "txvalidator_wasm.h"
#include <txvalidator.hpp>

#include <emscripten/bind.h>

#include <span>

namespace {

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

} // namespace

TxError VerifyScriptWASM(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags)
{
    static const bsv::CTxValidator validator{"main"};
    return validator.VerifyScript(
        std::span<const uint8_t>{extendedTX},
        std::span<const int32_t>{utxoHeights},
        blockHeight,
        consensus,
        std::span<const uint32_t>{customFlags});
}

TxError VerifyScriptArrayWASM(
    const emscripten::val& extendedTX,
    const emscripten::val& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const emscripten::val& customFlags)
{
    const std::vector<uint8_t> extendedTXVector = CopyNumberArray<uint8_t>(extendedTX);
    const std::vector<int32_t> utxoHeightVector = CopyNumberArray<int32_t>(utxoHeights);
    const std::vector<uint32_t> customFlagVector = CopyNumberArray<uint32_t>(customFlags);
    return VerifyScriptWASM(
        extendedTXVector,
        utxoHeightVector,
        blockHeight,
        consensus,
        customFlagVector);
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
}
