#include "scriptengine_wasm.h"
#include <scriptengine.hpp>

#include <emscripten/bind.h>   // embind

#include <span>
#include <random>

int testRandomInt() {
    static std::random_device rd;        // Seed source
    static std::mt19937 gen(rd());       // Mersenne Twister engine
    static std::uniform_int_distribution<int> dist(0, 1000000); // Range [0, 1,000,000]
    return dist(gen);
}


// Wrapper implementation: converts vectors -> spans and forwards call to original library.
int VerifyScriptWASM(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags)
{
    //std::span<const uint8_t> tx_span(extendedTX.data(), extendedTX.size());
    //std::span<const int32_t> utxo_span(utxoHeights.data(), utxoHeights.size());
    //std::span<const uint32_t> flags_span(customFlags.data(), customFlags.size());

    // TODO: replace with the real call into libbdk_core.a
    return testRandomInt();
}

namespace esbind = emscripten;
// Embind bindings
EMSCRIPTEN_BINDINGS(bdk_module) {
    // register vector types so embind can convert JS arrays -> std::vector<T>
    esbind::register_vector<uint8_t>("VectorUInt8");
    esbind::register_vector<int32_t>("VectorInt32");
    esbind::register_vector<uint32_t>("VectorUInt32");

    // expose the free function as Module.VerifyScript(...)
    esbind::function("VerifyScript", &VerifyScriptWASM);
}
