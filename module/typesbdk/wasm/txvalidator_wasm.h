#pragma once

#include <cstdint>

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
    int32_t* output) noexcept;

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
    int32_t* output) noexcept;

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
    int32_t* output) noexcept;

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
    int32_t* output) noexcept;

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
    int32_t* output) noexcept;

}
