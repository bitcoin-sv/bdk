#include <bdkcgo/txvalidator_cgo.h>
#include <core/txvalidator.hpp>
#include <core/verifyarg.hpp>


int TxValidator_CPP_SCRIPT_ERR_ERROR_COUNT(){
    return bsv::CPP_SCRIPT_ERR_ERROR_COUNT();
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

TxValidatorCGO TxValidator_Create(const char* networkNamePtr, int networkNameLen){
    try {
        const std::string networkName(networkNamePtr, networkNameLen);
        return new bsv::CTxValidator(networkName);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << e.what() << std::endl;
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

uint64_t TxValidator_GetSigOpCount(TxValidatorCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool countP2SHSigOps, char** errStr) {
    try {
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };

        return static_cast<bsv::CTxValidator*>(cgoEngine)->GetSigOpCount(extendedTx, hUTXOs, blockHeight, countP2SHSigOps);
    }
    catch (const std::exception& e) {
        *errStr = strdup(e.what());
        return uint64_t{0};
    }
}

uint32_t TxValidator_CalculateFlags(TxValidatorCGO cgoEngine, int32_t utxoHeight, int32_t blockHeight, bool consensus)
{
    return static_cast<bsv::CTxValidator*>(cgoEngine)->CalculateFlags(utxoHeight, blockHeight, consensus);
}

TxError TxValidator_VerifyScript(TxValidatorCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus){
    std::span<const uint8_t> extendedTx;
    if (extendedTxPtr != nullptr && extendedTxLen > 0) {
        const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
        extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
    }
    std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };
    return static_cast<bsv::CTxValidator*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus);
}

TxError TxValidator_VerifyScriptWithCustomFlags(TxValidatorCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus, const uint32_t* cFlagsPtr, int cFlagsLen){
    std::span<const uint8_t> extendedTx;
    if (extendedTxPtr != nullptr && extendedTxLen > 0) {
        const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
        extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
    }
    std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };
    std::span<const uint32_t> cFlags{ cFlagsPtr, (size_t)cFlagsLen };
    return static_cast<bsv::CTxValidator*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus, cFlags);
}

TxError TxValidator_CheckTransaction(TxValidatorCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus) {
    try {
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }
        std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };
        return static_cast<bsv::CTxValidator*>(cgoEngine)->CheckTransaction(extendedTx, hUTXOs, blockHeight, consensus);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError* TxValidator_VerifyScriptBatch(TxValidatorCGO cgoEngine, VerifyBatchCGO cgoBatch, int* resultSize) {
    bsv::CTxValidator* engine = static_cast<bsv::CTxValidator*>(cgoEngine);
    bsv::VerifyBatch* batch = static_cast<bsv::VerifyBatch*>(cgoBatch);

    std::vector<TxError> results = engine->VerifyScriptBatch(*batch);

    const size_t size = results.size();
    *resultSize = static_cast<int>(size);

    TxError* resultArray = static_cast<TxError*>(malloc(size * sizeof(TxError)));
    if (resultArray == nullptr) {
        *resultSize = 0;
        return nullptr;
    }

    for (size_t i = 0; i < size; ++i) {
        resultArray[i] = results[i];
    }

    return resultArray;
}