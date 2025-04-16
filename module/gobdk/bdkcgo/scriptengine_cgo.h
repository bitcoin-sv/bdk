#ifndef __SCRIPTENGINE_CGO_H__
#define __SCRIPTENGINE_CGO_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * ScriptEngine_CPP_SCRIPT_ERR_ERROR_COUNT return the SCRIPT_ERR_ERROR_COUNT as defined in the C++ code
 * This is for the testing purpose, in case the list of error code has changed, then the tester in
 * other binding language will be notified by a test
 */
int ScriptEngine_CPP_SCRIPT_ERR_ERROR_COUNT();

/**
 * ScriptEngineCGO implement the cgo handler for ScriptEngine class
 */
typedef void* ScriptEngineCGO;

/**
 * Handle constructor and destructor
 */
ScriptEngineCGO ScriptEngine_Create(const char* networkNamePtr, int networkNameLen);
void ScriptEngine_Destroy(ScriptEngineCGO cgoEngine);

/**
 * Settings for script engine. It returns nullptr if the setting is ok
 * 
 * Otherwise it returns an error string as a C-String null terminated
 * Client code must delete the returned C-String
 */
const char* ScriptEngine_SetMaxOpsPerScriptPolicy(ScriptEngineCGO cgoEngine, int64_t maxOpsPerScriptPolicyIn);
const char* ScriptEngine_SetMaxScriptNumLengthPolicy(ScriptEngineCGO cgoEngine, int64_t maxScriptNumLengthIn);
const char* ScriptEngine_SetMaxScriptSizePolicy(ScriptEngineCGO cgoEngine, int64_t maxScriptSizePolicyIn);
const char* ScriptEngine_SetMaxPubKeysPerMultiSigPolicy(ScriptEngineCGO cgoEngine, int64_t maxPubKeysPerMultiSigIn);
const char* ScriptEngine_SetMaxStackMemoryUsage(ScriptEngineCGO cgoEngine, int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn);
const char* ScriptEngine_SetGenesisActivationHeight(ScriptEngineCGO cgoEngine, int32_t genesisActivationHeightIn);
const char* ScriptEngine_SetChronicleActivationHeight(ScriptEngineCGO cgoEngine, int32_t chronicleActivationHeightIn);

// Forward getter to GlobalConfig call
uint64_t ScriptEngine_GetMaxOpsPerScript(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool consensus);
uint64_t ScriptEngine_GetMaxScriptNumLength(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus);
uint64_t ScriptEngine_GetMaxScriptSize(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool isConsensus);
uint64_t ScriptEngine_GetMaxPubKeysPerMultiSig(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool consensus);
uint64_t ScriptEngine_GetMaxStackMemoryUsage(ScriptEngineCGO cgoEngine, bool isGenesisEnabled, bool consensus);
int32_t ScriptEngine_GetGenesisActivationHeight(ScriptEngineCGO cgoEngine);
int32_t ScriptEngine_GetChronicleActivationHeight(ScriptEngineCGO cgoEngine);

/*
 * CalculateFlags helper to calculate the flags manually
 * Use for test and debug purpose
 */
uint32_t ScriptEngine_CalculateFlags(ScriptEngineCGO cgoEngine, int32_t utxoHeight, int32_t blockHeight, bool consensus);

/*
 * VerifyScript take inputs in C-style and forward the input to the C++ call
 * It returns error code rather than error string for performant purpose.
 *
 * Inputs C-Style
 *   - Extended Transaction binary
 *   - Array of utxo heights ( required to calculate flags )
 *   - Block Height
 *   - Consensus toogle
 */
int ScriptEngine_VerifyScript(ScriptEngineCGO cgoEngine,
	const char* extendedTxPtr, int extendedTxLen,
	const int32_t* hUTXOsPtr, int hUTXOsLen,
	int32_t blockHeight, bool consensus
);

#ifdef __cplusplus
}
#endif

#endif /* __SCRIPTENGINE_CGO_H__ */