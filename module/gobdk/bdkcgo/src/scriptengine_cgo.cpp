#include <bdkcgo/scriptengine_cgo.h>
#include <core/scriptengine.hpp>


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