#include <global_scriptconfig.h>

#include <iostream>
#include <string>

#include <config.h>

const char* SetGlobalScriptConfig(
        unsigned long long maxOpsPerScriptPolicyIn,
        unsigned long long maxScriptNumLengthPolicyIn,
        unsigned long long maxScriptSizePolicyIn,
        unsigned long long maxPubKeysPerMultiSigIn,
        unsigned long long maxStackMemoryUsageConsensusIn,
        unsigned long long maxStackMemoryUsagePolicyIn
    ) {
        std::string err;
        auto& gConfig = GlobalConfig::GetModifiableGlobalConfig();
        auto ok1 = gConfig.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, &err);
        auto ok2 = gConfig.SetMaxScriptNumLengthPolicy(maxScriptNumLengthPolicyIn, &err);
        auto ok3 = gConfig.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, &err);
        auto ok4 = gConfig.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, &err);
        auto ok5 = gConfig.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, &err);

        auto ok = (ok1 &&  ok2 && ok3 && ok4 && ok5);
        if (err.empty() && !ok ) {
            err = "Unknown error while setting global config for script";
        }

        if (err.empty()) {
            return nullptr;
        }

        char* cstr = new char[err.size() + 1];// +1 for the null terminator
        std::strcpy(cstr, err.c_str());
        return cstr;
    }