#ifndef __GLOBAL_SCRIPTCONFIG_H__
#define __GLOBAL_SCRIPTCONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set ScriptConfig globally for the core C++. It return the error string if any
 * 
 * The caller is responsible to free memory
 */
const char* SetGlobalScriptConfig(
        unsigned long long maxOpsPerScriptPolicy,
        unsigned long long maxScriptNumLengthPolicy,
        unsigned long long maxScriptSizePolicy,
        unsigned long long maxPubKeysPerMultiSig,
        unsigned long long maxStackMemoryUsageConsensus,
        unsigned long long maxStackMemoryUsagePolicy
    );

#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL_SCRIPTCONFIG_H__ */