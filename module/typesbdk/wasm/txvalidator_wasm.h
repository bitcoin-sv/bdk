#pragma once
#include <vector>
#include <cstdint>
#include <txerror.h>

TxError VerifyScriptWASM(
    const std::vector<uint8_t>& extendedTX,
    const std::vector<int32_t>& utxoHeights,
    int32_t blockHeight,
    bool consensus,
    const std::vector<uint32_t>& customFlags = std::vector<uint32_t>());
