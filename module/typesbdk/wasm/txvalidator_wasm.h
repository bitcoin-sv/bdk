#pragma once

#include <cstdint>

extern "C" {

void bdk_prepare_verification() noexcept;
uint32_t bdk_prepare_signing() noexcept;
uint32_t bdk_verification_table_snapshot_size() noexcept;
uint32_t bdk_export_verification_tables(uint8_t* output, uint32_t size) noexcept;
uint32_t bdk_import_verification_tables(
    const uint8_t* input,
    uint32_t size) noexcept;

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

uint32_t bdk_sign_digest(
    const uint8_t* privateKey,
    const uint8_t* digest,
    uint8_t* signature) noexcept;

uint32_t bdk_verify_digest(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* digest,
    const uint8_t* signature,
    uint32_t signatureSize) noexcept;

void bdk_verify_digest_batch(
    const uint8_t* publicKeys,
    uint32_t publicKeySize,
    const uint32_t* publicKeyOffsets,
    const uint8_t* digests,
    uint32_t digestSize,
    const uint8_t* signatures,
    uint32_t signatureSize,
    const uint32_t* signatureOffsets,
    uint32_t entryCount,
    uint8_t* output) noexcept;

uint32_t bdk_public_key_from_private(
    const uint8_t* privateKey,
    uint8_t* publicKey) noexcept;

uint32_t bdk_multiply_public_key(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* scalar,
    uint8_t* multipliedPublicKey) noexcept;

uint32_t bdk_tweak_public_key_add(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* tweak,
    uint8_t* tweakedPublicKey) noexcept;

uint32_t bdk_tweak_private_key_add(
    const uint8_t* privateKey,
    const uint8_t* tweak,
    uint8_t* tweakedPrivateKey) noexcept;

}
