#ifndef __GLOBAL_SCRIPTCONFIG_H__
#define __GLOBAL_SCRIPTCONFIG_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set ScriptConfig globally for the core C++. It return the error string if any
 * 
 * The caller is responsible to free memory of the returned char*
 */
const char* CgoSetGlobalScriptConfig(
        const char* chainNetwork,
        int64_t maxOpsPerScriptPolicy,
        int64_t maxScriptNumLengthPolicy,
        int64_t maxScriptSizePolicy,
        int64_t maxPubKeysPerMultiSig,
        int64_t maxStackMemoryUsageConsensus,
        int64_t maxStackMemoryUsagePolicy,
        int32_t customGenesisHeight
    );

/**
 * Get custom genefis activation height being set in the global config
 */
int32_t CgoGetGenesisActivationHeight();

#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL_SCRIPTCONFIG_H__ */