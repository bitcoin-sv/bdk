#pragma once
#include <vector>
#include <cstdint>

// Expose a simple C++ API you will bind to JS.
// You can adapt names/types if you prefer class-based API.
int VerifyScriptWASM(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags = std::vector<uint32_t>());