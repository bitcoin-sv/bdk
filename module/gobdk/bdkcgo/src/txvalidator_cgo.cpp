#include <bdkcgo/txvalidator_cgo.h>
#include <bdkcgo/src/abiguard.hpp>
#include <core/txvalidator.hpp>
#include <core/validatearg.hpp>


int TxValidator_CPP_SCRIPT_ERR_ERROR_COUNT(){
    return bsv::CPP_SCRIPT_ERR_ERROR_COUNT();
}

uint64_t TxValidator_ABI_EchoLength(uint64_t len){
    return len;
}

// Internal helper to emit a diagnostic from a catch handler.
// Stream insertion and the flush in std::endl can allocate, so they can throw — and a
// catch handler that throws leaves an extern "C" function with an exception already in
// flight, which is undefined behaviour. The emission is therefore best effort: if it
// fails, the diagnostic is dropped and the caller still gets its documented result.
static void _helper_log_cgo_exception(const char* where, const char* what) noexcept {
    try {
        std::cout << "CGO EXCEPTION : " << where << " " << what << std::endl;
    }
    catch (...) {
    }
}

// Internal helper to convert std::string to const char*
const char* _helper_string2char(const std::string& stdStr) {
    if (stdStr.empty()) {
        return nullptr;
    }

    char* charPtr = nullptr;
    charPtr = new char[stdStr.size() + 1];
    std::strcpy(charPtr, stdStr.c_str());
    return charPtr;
}

TxValidatorCGO TxValidator_CreateV2(const char* networkNamePtr, uint64_t networkNameLen){
    try {
        const std::span<const uint8_t> nameSpan = bdkcgo::byte_span(networkNamePtr, networkNameLen);
        std::string networkName;
        if (!nameSpan.empty()) {
            networkName.assign(reinterpret_cast<const char*>(nameSpan.data()), nameSpan.size());
        }
        return new bsv::CTxValidator(networkName);
    }
    catch (const std::exception& e) {
        _helper_log_cgo_exception(__FILE__ " at TxValidator_CreateV2", e.what());
        return nullptr;
    }
    catch (...) {
        // Nothing may cross the extern "C" boundary; a non-std exception takes the
        // same channel as any other construction failure.
        _helper_log_cgo_exception(__FILE__ " at TxValidator_CreateV2", "unknown exception");
        return nullptr;
    }
}

void TxValidator_Destroy(TxValidatorCGO cgoEngine)
{
    bsv::CTxValidator* ptr = static_cast<bsv::CTxValidator*>(cgoEngine);
    delete ptr;
}

const char* TxValidator_SetMaxOpsPerScriptPolicy(TxValidatorCGO cgoEngine, int64_t maxOpsPerScriptPolicyIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxScriptNumLengthPolicy(TxValidatorCGO cgoEngine, int64_t maxScriptNumLengthIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxScriptSizePolicy(TxValidatorCGO cgoEngine, int64_t maxScriptSizePolicyIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxScriptSizePolicy(maxScriptSizePolicyIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxPubKeysPerMultiSigPolicy(TxValidatorCGO cgoEngine, int64_t maxPubKeysPerMultiSigIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxStackMemoryUsage(TxValidatorCGO cgoEngine, int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetGenesisActivationHeight(TxValidatorCGO cgoEngine, int32_t genesisActivationHeightIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetGenesisActivationHeight(genesisActivationHeightIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetChronicleActivationHeight(TxValidatorCGO cgoEngine, int32_t chronicleActivationHeightIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetChronicleActivationHeight(chronicleActivationHeightIn, &err);
    return _helper_string2char(err);
}

uint64_t TxValidator_GetMaxOpsPerScript(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxOpsPerScript(isGenesisEnabled, isConsensus);
}

uint64_t TxValidator_GetMaxScriptNumLength(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxScriptNumLength(isGenesisEnabled, isChronicleEnabled, isConsensus);
}

uint64_t TxValidator_GetMaxScriptSize(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t TxValidator_GetMaxPubKeysPerMultiSig(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t TxValidator_GetMaxStackMemoryUsage(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t TxValidator_GetGenesisActivationHeight(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetGenesisActivationHeight();
}

int32_t TxValidator_GetChronicleActivationHeight(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetChronicleActivationHeight();
}

uint64_t TxValidator_GetGenesisGracefulPeriod(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetGenesisGracefulPeriod();
}

uint64_t TxValidator_GetChronicleGracefulPeriod(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetChronicleGracefulPeriod();
}

const char* TxValidator_SetGenesisGracefulPeriod(TxValidatorCGO cgoEngine, int64_t genesisGracefulPeriodIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetGenesisGracefulPeriod(genesisGracefulPeriodIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetChronicleGracefulPeriod(TxValidatorCGO cgoEngine, int64_t chronicleGracefulPeriodIn)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetChronicleGracefulPeriod(chronicleGracefulPeriodIn, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxTxSizePolicy(TxValidatorCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxTxSizePolicy(value, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxSigOpsPostGenesisPolicy(TxValidatorCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxSigOpsPostGenesisPolicy(value, &err);
    return _helper_string2char(err);
}

void TxValidator_SetMaxSigOpsPolicy(TxValidatorCGO cgoEngine, uint64_t value)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxSigOpsPolicy(value);
}

const char* TxValidator_SetMinConsolidationFactor(TxValidatorCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMinConsolidationFactor(value, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMaxConsolidationInputScriptSize(TxValidatorCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMaxConsolidationInputScriptSize(value, &err);
    return _helper_string2char(err);
}

const char* TxValidator_SetMinConfConsolidationInput(TxValidatorCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMinConfConsolidationInput(value, &err);
    return _helper_string2char(err);
}

void TxValidator_SetAcceptNonStdConsolidationInput(TxValidatorCGO cgoEngine, bool value)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetAcceptNonStdConsolidationInput(value);
}

const char* TxValidator_SetMinMiningTxFee(TxValidatorCGO cgoEngine, int64_t satoshisPerKB)
{
    std::string err;
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetMinMiningTxFee(satoshisPerKB, &err);
    return _helper_string2char(err);
}

void TxValidator_SetDataCarrierSize(TxValidatorCGO cgoEngine, uint64_t dataCarrierSize)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetDataCarrierSize(dataCarrierSize);
}

void TxValidator_SetDataCarrier(TxValidatorCGO cgoEngine, bool dataCarrier)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetDataCarrier(dataCarrier);
}

void TxValidator_SetAcceptNonStandardOutput(TxValidatorCGO cgoEngine, bool accept)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetAcceptNonStandardOutput(accept);
}

void TxValidator_SetRequireStandard(TxValidatorCGO cgoEngine, bool require)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetRequireStandard(require);
}

void TxValidator_SetPermitBareMultisig(TxValidatorCGO cgoEngine, bool permit)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->SetPermitBareMultisig(permit);
}

void TxValidator_ResetDefault(TxValidatorCGO cgoEngine)
{
    static_cast<bsv::CTxValidator*>(cgoEngine)->ResetDefault();
}

uint64_t TxValidator_GetMaxTxSize(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetMaxTxSize(isGenesisEnabled, isChronicleEnabled, isConsensus);
}

uint64_t TxValidator_GetDataCarrierSize(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetDataCarrierSize();
}

bool TxValidator_GetDataCarrier(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetDataCarrier();
}

bool TxValidator_GetAcceptNonStandardOutput(TxValidatorCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetAcceptNonStandardOutput(isGenesisEnabled, isChronicleEnabled);
}

bool TxValidator_GetRequireStandard(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetRequireStandard();
}

bool TxValidator_GetPermitBareMultisig(TxValidatorCGO cgoEngine)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->GetPermitBareMultisig();
}

uint64_t TxValidator_GetSigOpCount(TxValidatorCGO cgoEngine, const char* extendedTxPtr, uint64_t extendedTxLen, const int32_t* hUTXOsPtr, uint64_t hUTXOsLen, int32_t blockHeight, bool countP2SHSigOps, bool consensus, char** errStr) {
    try {
        const std::span<const uint8_t> extendedTx = bdkcgo::byte_span(extendedTxPtr, extendedTxLen);
        const std::span<const int32_t> hUTXOs = bdkcgo::int32_span(hUTXOsPtr, hUTXOsLen);

        return static_cast<bsv::CTxValidator*>(cgoEngine)->GetSigOpCount(extendedTx, hUTXOs, blockHeight, countP2SHSigOps, consensus);
    }
    catch (const std::exception& e) {
        // This entry point has no TxError channel: an ABI rejection is reported as a
        // diagnostic string with a count of 0, like any other failure here.
        if (errStr != nullptr) {
            *errStr = strdup(e.what());
        }
        return uint64_t{0};
    }
    catch (...) {
        // Same channel for an exception carrying no message, so that nothing escapes.
        if (errStr != nullptr) {
            *errStr = strdup("unknown exception at the cgo boundary");
        }
        return uint64_t{0};
    }
}

uint32_t TxValidator_CalculateFlags(TxValidatorCGO cgoEngine, int32_t utxoHeight, int32_t blockHeight, bool consensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->CalculateFlags(utxoHeight, blockHeight, consensus);
}

TxError TxValidator_VerifyScript(TxValidatorCGO cgoEngine, const char* extendedTxPtr, uint64_t extendedTxLen, const int32_t* hUTXOsPtr, uint64_t hUTXOsLen, int32_t blockHeight, bool consensus){
    try {
        const std::span<const uint8_t> extendedTx = bdkcgo::byte_span(extendedTxPtr, extendedTxLen);
        const std::span<const int32_t> hUTXOs = bdkcgo::int32_span(hUTXOsPtr, hUTXOsLen);
        return static_cast<bsv::CTxValidator*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus);
    }
    catch (const bdkcgo::AbiArgumentError& e) {
        return bsv::AbiErrorToTxError(e.code());
    }
    catch (...) {
        return bsv::TxErrorException();
    }
}

TxError TxValidator_VerifyScriptWithCustomFlags(TxValidatorCGO cgoEngine, const char* extendedTxPtr, uint64_t extendedTxLen, const int32_t* hUTXOsPtr, uint64_t hUTXOsLen, int32_t blockHeight, bool consensus, const uint32_t* cFlagsPtr, uint64_t cFlagsLen){
    try {
        const std::span<const uint8_t> extendedTx = bdkcgo::byte_span(extendedTxPtr, extendedTxLen);
        const std::span<const int32_t> hUTXOs = bdkcgo::int32_span(hUTXOsPtr, hUTXOsLen);
        const std::span<const uint32_t> cFlags = bdkcgo::uint32_span(cFlagsPtr, cFlagsLen);
        return static_cast<bsv::CTxValidator*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus, cFlags);
    }
    catch (const bdkcgo::AbiArgumentError& e) {
        return bsv::AbiErrorToTxError(e.code());
    }
    catch (...) {
        return bsv::TxErrorException();
    }
}

TxError TxValidator_ValidateTransaction(TxValidatorCGO cgoEngine, const char* extendedTxPtr, uint64_t extendedTxLen, const int32_t* hUTXOsPtr, uint64_t hUTXOsLen, int32_t blockHeight, bool consensus) {
    try {
        const std::span<const uint8_t> extendedTx = bdkcgo::byte_span(extendedTxPtr, extendedTxLen);
        const std::span<const int32_t> hUTXOs = bdkcgo::int32_span(hUTXOsPtr, hUTXOsLen);
        return static_cast<bsv::CTxValidator*>(cgoEngine)->ValidateTransaction(extendedTx, hUTXOs, blockHeight, consensus);
    }
    catch (const bdkcgo::AbiArgumentError& e) {
        return bsv::AbiErrorToTxError(e.code());
    }
    catch (...) {
        return bsv::TxErrorException();
    }
}

// Internal helper building the single-element array TxValidator_ValidateBatch uses to
// report a failure that has no per-entry result to attach itself to.
static TxError* _helper_single_result(TxError result, uint64_t* resultSize) {
    TxError* resultArray = static_cast<TxError*>(malloc(sizeof(TxError)));
    if (resultArray == nullptr) {
        *resultSize = 0;
        return nullptr;
    }
    resultArray[0] = result;
    *resultSize = 1;
    return resultArray;
}

TxError* TxValidator_ValidateBatch(TxValidatorCGO cgoEngine, ValidateBatchCGO cgoBatch, uint64_t* resultSize) {
    if (resultSize == nullptr) {
        return nullptr;
    }
    *resultSize = 0;

    // The whole operation is inside the try: reserving and filling the result vector
    // can throw bad_alloc, and an exception leaving an extern "C" function is
    // undefined behaviour. Anything that escapes is reported through this function's
    // own single-result convention.
    try {
        bsv::CTxValidator* engine = static_cast<bsv::CTxValidator*>(cgoEngine);
        bsv::ValidateBatch* batch = static_cast<bsv::ValidateBatch*>(cgoBatch);

        std::vector<TxError> results = engine->ValidateBatch(*batch);

        const size_t size = results.size();
        if (size == 0) {
            return nullptr;
        }

        // The result count comes from the batch, but the allocation is sized by it: check
        // the multiplication before performing it.
        if (size > SIZE_MAX / sizeof(TxError)) {
            return _helper_single_result(bsv::AbiErrorToTxError(bsv::AbiError_t::LengthOverflow), resultSize);
        }

        TxError* resultArray = static_cast<TxError*>(malloc(size * sizeof(TxError)));
        if (resultArray == nullptr) {
            return nullptr;
        }
        *resultSize = static_cast<uint64_t>(size);

        for (size_t i = 0; i < size; ++i) {
            resultArray[i] = results[i];
        }

        return resultArray;
    }
    catch (const bdkcgo::AbiArgumentError& e) {
        return _helper_single_result(bsv::AbiErrorToTxError(e.code()), resultSize);
    }
    catch (...) {
        return _helper_single_result(bsv::TxErrorException(), resultSize);
    }
}