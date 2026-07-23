#include "txvalidator_wasm.h"
#include <crypto/sha256.h>
#include <txvalidator.hpp>
#include <secp256k1.h>
#include <secp256k1_ecdh.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

extern "C" void bdk_secp256k1_prepare_verification_tables(void);
extern "C" size_t bdk_secp256k1_verification_table_snapshot_size(void);
extern "C" int bdk_secp256k1_export_verification_tables(
    unsigned char* output,
    size_t size);
extern "C" int bdk_secp256k1_import_verification_tables(
    const unsigned char* input,
    size_t size);

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

secp256k1_context* SigningContextStorage() noexcept
{
    static secp256k1_context* context =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    return context;
}

bool& SigningContextPrepared() noexcept
{
    static bool prepared = false;
    return prepared;
}

const secp256k1_context* SigningContext() noexcept
{
    return SigningContextPrepared() ? SigningContextStorage() : nullptr;
}

bool ParsePublicKey(
    const uint8_t* bytes,
    uint32_t size,
    secp256k1_pubkey& publicKey) noexcept
{
    return bytes != nullptr && (size == 33 || size == 65) &&
        secp256k1_ec_pubkey_parse(
            secp256k1_context_static, &publicKey, bytes, size) == 1;
}

bool SerializePublicKey(
    const secp256k1_pubkey& publicKey,
    uint8_t* output) noexcept
{
    if(output == nullptr)
        return false;
    size_t size = 33;
    return secp256k1_ec_pubkey_serialize(
        secp256k1_context_static,
        output,
        &size,
        &publicKey,
        SECP256K1_EC_COMPRESSED) == 1 && size == 33;
}

int SerializeCompressedPoint(
    uint8_t* output,
    const uint8_t* x,
    const uint8_t* y,
    void*) noexcept
{
    output[0] = static_cast<uint8_t>(2 | (y[31] & 1));
    std::memcpy(output + 1, x, 32);
    return 1;
}

bool VerifyDigest(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* digest,
    const uint8_t* signature,
    uint32_t signatureSize) noexcept
{
    if(digest == nullptr || signature == nullptr || signatureSize == 0)
        return false;
    secp256k1_pubkey parsedPublicKey;
    secp256k1_ecdsa_signature parsedSignature;
    if(!ParsePublicKey(publicKey, publicKeySize, parsedPublicKey) ||
       secp256k1_ecdsa_signature_parse_der(
           secp256k1_context_static,
           &parsedSignature,
           signature,
           signatureSize) != 1) {
        return false;
    }
    /* The SDK's generic ECDSA verifier accepts the mathematically equivalent
     * high-S form. Script policy enforces LOW_S separately inside the
     * transaction interpreter, so keep this generic digest primitive
     * semantically compatible by normalizing before verification. */
    secp256k1_ecdsa_signature_normalize(
        secp256k1_context_static,
        &parsedSignature,
        &parsedSignature);
    bdk_secp256k1_prepare_verification_tables();
    return secp256k1_ecdsa_verify(
        secp256k1_context_static,
        &parsedSignature,
        digest,
        &parsedPublicKey) == 1;
}

} // namespace

extern "C" {

int bdk_secp256k1_verification_snapshot_is_valid(
    const unsigned char* input,
    size_t size) noexcept
{
    /* Bind imports to the complete canonical W15 table, including its build
     * geometry and internal field representation. */
    static constexpr std::array<uint8_t, CSHA256::OUTPUT_SIZE> expected{
        0xd3, 0xc8, 0x9d, 0x8b, 0xa8, 0x29, 0xac, 0x39,
        0x0e, 0xe9, 0x18, 0xc1, 0x21, 0x91, 0xcf, 0x0f,
        0x53, 0x70, 0x71, 0x6b, 0x28, 0x51, 0x41, 0xec,
        0x70, 0xfe, 0x21, 0x5f, 0x3b, 0x9d, 0xd6, 0x49
    };
    if(input == nullptr)
        return 0;
    std::array<uint8_t, CSHA256::OUTPUT_SIZE> digest{};
    CSHA256{}.Write(input, size).Finalize(digest);
    return std::equal(digest.begin(), digest.end(), expected.begin()) ? 1 : 0;
}

void bdk_prepare_verification() noexcept
{
    bdk_secp256k1_prepare_verification_tables();
}

uint32_t bdk_prepare_signing(const uint8_t* seed) noexcept
{
    secp256k1_context* context = SigningContextStorage();
    if(seed == nullptr || context == nullptr ||
       secp256k1_context_randomize(context, seed) != 1) {
        return 0;
    }
    SigningContextPrepared() = true;
    return 1;
}

uint32_t bdk_verification_table_snapshot_size() noexcept
{
    const size_t size = bdk_secp256k1_verification_table_snapshot_size();
    return size <= UINT32_MAX ? static_cast<uint32_t>(size) : 0;
}

uint32_t bdk_export_verification_tables(
    uint8_t* output,
    uint32_t size) noexcept
{
    return bdk_secp256k1_export_verification_tables(output, size) == 1 ? 1 : 0;
}

uint32_t bdk_import_verification_tables(
    const uint8_t* input,
    uint32_t size) noexcept
{
    return bdk_secp256k1_import_verification_tables(input, size) == 1 ? 1 : 0;
}

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

uint32_t bdk_sign_digest(
    const uint8_t* privateKey,
    const uint8_t* digest,
    uint8_t* signature) noexcept
{
    if(privateKey == nullptr || digest == nullptr || signature == nullptr)
        return 0;
    const secp256k1_context* context = SigningContext();
    if(context == nullptr)
        return 0;
    secp256k1_ecdsa_signature parsedSignature;
    if(secp256k1_ecdsa_sign(
           context, &parsedSignature, digest, privateKey, nullptr, nullptr) != 1) {
        return 0;
    }
    size_t size = 72;
    return secp256k1_ecdsa_signature_serialize_der(
        context, signature, &size, &parsedSignature) == 1
        ? static_cast<uint32_t>(size)
        : 0;
}

uint32_t bdk_verify_digest(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* digest,
    const uint8_t* signature,
    uint32_t signatureSize) noexcept
{
    return VerifyDigest(
        publicKey, publicKeySize, digest, signature, signatureSize) ? 1 : 0;
}

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
    uint8_t* output) noexcept
{
    if(output == nullptr)
        return;
    if(digestSize != entryCount * 32 ||
       (publicKeySize != 0 && publicKeys == nullptr) ||
       (signatureSize != 0 && signatures == nullptr) ||
       (digestSize != 0 && digests == nullptr) ||
       !ValidOffsets(publicKeyOffsets, entryCount, publicKeySize) ||
       !ValidOffsets(signatureOffsets, entryCount, signatureSize)) {
        std::memset(output, 0, entryCount);
        return;
    }
    for(uint32_t index = 0; index < entryCount; ++index) {
        const auto publicKey =
            Slice(publicKeys, publicKeyOffsets, index);
        const auto signature =
            Slice(signatures, signatureOffsets, index);
        output[index] = VerifyDigest(
            publicKey.data(),
            static_cast<uint32_t>(publicKey.size()),
            digests + index * 32,
            signature.data(),
            static_cast<uint32_t>(signature.size())) ? 1 : 0;
    }
}

uint32_t bdk_public_key_from_private(
    const uint8_t* privateKey,
    uint8_t* publicKey) noexcept
{
    if(privateKey == nullptr || publicKey == nullptr)
        return 0;
    const secp256k1_context* context = SigningContext();
    secp256k1_pubkey parsedPublicKey;
    return context != nullptr &&
        secp256k1_ec_pubkey_create(context, &parsedPublicKey, privateKey) == 1 &&
        SerializePublicKey(parsedPublicKey, publicKey) ? 1 : 0;
}

uint32_t bdk_multiply_public_key(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* scalar,
    uint8_t* multipliedPublicKey) noexcept
{
    if(scalar == nullptr || multipliedPublicKey == nullptr)
        return 0;
    secp256k1_pubkey parsedPublicKey;
    return ParsePublicKey(publicKey, publicKeySize, parsedPublicKey) &&
        secp256k1_ecdh(
            secp256k1_context_static,
            multipliedPublicKey,
            &parsedPublicKey,
            scalar,
            SerializeCompressedPoint,
            nullptr) == 1 ? 1 : 0;
}

uint32_t bdk_tweak_public_key_add(
    const uint8_t* publicKey,
    uint32_t publicKeySize,
    const uint8_t* tweak,
    uint8_t* tweakedPublicKey) noexcept
{
    if(tweak == nullptr || tweakedPublicKey == nullptr)
        return 0;
    secp256k1_pubkey parsedPublicKey;
    if(!ParsePublicKey(publicKey, publicKeySize, parsedPublicKey))
        return 0;
    return secp256k1_ec_pubkey_tweak_add(
               secp256k1_context_static, &parsedPublicKey, tweak) == 1 &&
        SerializePublicKey(parsedPublicKey, tweakedPublicKey) ? 1 : 0;
}

uint32_t bdk_tweak_private_key_add(
    const uint8_t* privateKey,
    const uint8_t* tweak,
    uint8_t* tweakedPrivateKey) noexcept
{
    if(privateKey == nullptr || tweak == nullptr || tweakedPrivateKey == nullptr)
        return 0;
    std::memcpy(tweakedPrivateKey, privateKey, 32);
    return secp256k1_ec_seckey_tweak_add(
        secp256k1_context_static, tweakedPrivateKey, tweak) == 1 ? 1 : 0;
}

} // extern "C"
