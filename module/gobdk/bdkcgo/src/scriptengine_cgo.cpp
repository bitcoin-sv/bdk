#include <bdkcgo/scriptengine_cgo.h>
#include <core/scriptengine.hpp>
#include <core/verifyarg.hpp>


int ScriptEngine_CPP_SCRIPT_ERR_ERROR_COUNT(){
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

ScriptEngineCGO ScriptEngine_Create(const char* networkNamePtr, int networkNameLen){
    try {
        const std::string networkName(networkNamePtr, networkNameLen);
        return new bsv::CScriptEngine(networkName);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << e.what() << std::endl;
        return nullptr;
    }
}

void ScriptEngine_Destroy(ScriptEngineCGO cgoEngine)
{
    bsv::CScriptEngine* ptr = static_cast<bsv::CScriptEngine*>(cgoEngine);
    delete ptr;
}

const char* ScriptEngine_SetMaxOpsPerScriptPolicy(ScriptEngineCGO cgoEngine, int64_t maxOpsPerScriptPolicyIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetMaxScriptNumLengthPolicy(ScriptEngineCGO cgoEngine, int64_t maxScriptNumLengthIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetMaxScriptSizePolicy(ScriptEngineCGO cgoEngine, int64_t maxScriptSizePolicyIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxScriptSizePolicy(maxScriptSizePolicyIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetMaxPubKeysPerMultiSigPolicy(ScriptEngineCGO cgoEngine, int64_t maxPubKeysPerMultiSigIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetMaxStackMemoryUsage(ScriptEngineCGO cgoEngine, int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetGenesisActivationHeight(ScriptEngineCGO cgoEngine, int32_t genesisActivationHeightIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetGenesisActivationHeight(genesisActivationHeightIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetChronicleActivationHeight(ScriptEngineCGO cgoEngine, int32_t chronicleActivationHeightIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetChronicleActivationHeight(chronicleActivationHeightIn, &err);
    return _helper_string2char(err);
}

uint64_t ScriptEngine_GetMaxOpsPerScript(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxOpsPerScript(isGenesisEnabled, isConsensus);
}

uint64_t ScriptEngine_GetMaxScriptNumLength(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxScriptNumLength(isGenesisEnabled, isChronicleEnabled, isConsensus);
}

uint64_t ScriptEngine_GetMaxScriptSize(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t ScriptEngine_GetMaxPubKeysPerMultiSig(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t ScriptEngine_GetMaxStackMemoryUsage(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t ScriptEngine_GetGenesisActivationHeight(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetGenesisActivationHeight();
}

int32_t ScriptEngine_GetChronicleActivationHeight(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetChronicleActivationHeight();
}

uint64_t ScriptEngine_GetGenesisGracefulPeriod(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetGenesisGracefulPeriod();
}

uint64_t ScriptEngine_GetChronicleGracefulPeriod(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetChronicleGracefulPeriod();
}

const char* ScriptEngine_SetGenesisGracefulPeriod(ScriptEngineCGO cgoEngine, int64_t genesisGracefulPeriodIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetGenesisGracefulPeriod(genesisGracefulPeriodIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetChronicleGracefulPeriod(ScriptEngineCGO cgoEngine, int64_t chronicleGracefulPeriodIn)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetChronicleGracefulPeriod(chronicleGracefulPeriodIn, &err);
    return _helper_string2char(err);
}

const char* ScriptEngine_SetMaxTxSizePolicy(ScriptEngineCGO cgoEngine, int64_t value)
{
    std::string err;
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetMaxTxSizePolicy(value, &err);
    return _helper_string2char(err);
}

void ScriptEngine_SetDataCarrierSize(ScriptEngineCGO cgoEngine, uint64_t dataCarrierSize)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetDataCarrierSize(dataCarrierSize);
}

void ScriptEngine_SetDataCarrier(ScriptEngineCGO cgoEngine, bool dataCarrier)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetDataCarrier(dataCarrier);
}

void ScriptEngine_SetAcceptNonStandardOutput(ScriptEngineCGO cgoEngine, bool accept)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetAcceptNonStandardOutput(accept);
}

void ScriptEngine_SetRequireStandard(ScriptEngineCGO cgoEngine, bool require)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetRequireStandard(require);
}

void ScriptEngine_SetPermitBareMultisig(ScriptEngineCGO cgoEngine, bool permit)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->SetPermitBareMultisig(permit);
}

void ScriptEngine_ResetDefault(ScriptEngineCGO cgoEngine)
{
    static_cast<bsv::CScriptEngine*>(cgoEngine)->ResetDefault();
}

uint64_t ScriptEngine_GetMaxTxSize(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetMaxTxSize(isGenesisEnabled, isChronicleEnabled, isConsensus);
}

uint64_t ScriptEngine_GetDataCarrierSize(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetDataCarrierSize();
}

bool ScriptEngine_GetDataCarrier(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetDataCarrier();
}

bool ScriptEngine_GetAcceptNonStandardOutput(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetAcceptNonStandardOutput(isGenesisEnabled, isChronicleEnabled);
}

bool ScriptEngine_GetRequireStandard(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetRequireStandard();
}

bool ScriptEngine_GetPermitBareMultisig(ScriptEngineCGO cgoEngine)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetPermitBareMultisig();
}

uint64_t ScriptEngine_GetSigOpCount(ScriptEngineCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, char** errStr) {
    try {
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };

        return static_cast<bsv::CScriptEngine*>(cgoEngine)->GetSigOpCount(extendedTx, hUTXOs, blockHeight);
    }
    catch (const std::exception& e) {
        *errStr = strdup(e.what());
        return uint64_t{0};
    }
}

uint32_t ScriptEngine_CalculateFlags(ScriptEngineCGO cgoEngine, int32_t utxoHeight, int32_t blockHeight, bool consensus)
{
    return static_cast<bsv::CScriptEngine*>(cgoEngine)->CalculateFlags(utxoHeight, blockHeight, consensus);
}

int ScriptEngine_VerifyScript(ScriptEngineCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus){
    try {
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };

        return static_cast<bsv::CScriptEngine*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__  << e.what() << std::endl;
        return SCRIPT_ERR_ERROR_COUNT + 1;
    }
}

int ScriptEngine_VerifyScriptWithCustomFlags(ScriptEngineCGO cgoEngine, const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus, const uint32_t* cFlagsPtr, int cFlagsLen){
    try {
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs{ hUTXOsPtr, (size_t)hUTXOsLen };
        std::span<const uint32_t> cFlags{ cFlagsPtr, (size_t)cFlagsLen };

        return static_cast<bsv::CScriptEngine*>(cgoEngine)->VerifyScript(extendedTx, hUTXOs, blockHeight, consensus, cFlags);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__  << e.what() << std::endl;
        return SCRIPT_ERR_ERROR_COUNT + 1;
    }
}

int* ScriptEngine_VerifyScriptBatch(ScriptEngineCGO cgoEngine, VerifyBatchCGO cgoBatch, int* resultSize) {
    try {
        bsv::CScriptEngine* engine = static_cast<bsv::CScriptEngine*>(cgoEngine);
        bsv::VerifyBatch* batch = static_cast<bsv::VerifyBatch*>(cgoBatch);

        // Call the C++ VerifyScriptBatch method
        std::vector<ScriptError> results = engine->VerifyScriptBatch(*batch);

        // Allocate C array for results
        const size_t size = results.size();
        *resultSize = static_cast<int>(size);

        int* resultArray = static_cast<int*>(malloc(size * sizeof(int)));
        if (resultArray == nullptr) {
            *resultSize = 0;
            return nullptr;
        }

        // Copy results to C array
        for (size_t i = 0; i < size; ++i) {
            resultArray[i] = static_cast<int>(results[i]);
        }

        return resultArray;
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << " " << e.what() << std::endl;
        *resultSize = 0;
        return nullptr;
    }
}