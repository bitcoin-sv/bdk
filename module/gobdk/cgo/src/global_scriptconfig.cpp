#include <iostream>
#include <string>

#include <config.h>
#include <cgo/global_scriptconfig.h>


const char* SetGlobalScriptConfig(
        unsigned long long maxOpsPerScriptPolicyIn,
        unsigned long long maxScriptNumLengthPolicyIn,
        unsigned long long maxScriptSizePolicyIn,
        unsigned long long maxPubKeysPerMultiSigIn,
        unsigned long long maxStackMemoryUsageConsensusIn,
        unsigned long long maxStackMemoryUsagePolicyIn
    ) {
        const char* cstr = nullptr;

        try {
            std::string err;
            bool ok=true;
            auto& gConfig = GlobalConfig::GetModifiableGlobalConfig();
            if (maxOpsPerScriptPolicyIn > 0)
                ok = ok && gConfig.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, &err);
            if (maxScriptNumLengthPolicyIn > 0)
                ok = ok && gConfig.SetMaxScriptNumLengthPolicy(maxScriptNumLengthPolicyIn, &err);
            if (maxScriptSizePolicyIn > 0)
                ok = ok && gConfig.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, &err);
            if (maxPubKeysPerMultiSigIn > 0)
                ok = ok &&  gConfig.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, &err);
            if (maxStackMemoryUsageConsensusIn > 0 || maxStackMemoryUsagePolicyIn > 0)
                ok = ok && gConfig.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, &err);

            if (err.empty() && !ok ) {
                err = "Unknown error while setting global config for script";
            }

            if (err.empty()) {
                return nullptr;
            }

            char* cstr = new char[err.size() + 1];// +1 for the null terminator
            std::strcpy(cstr, err.c_str());
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ << " "<< e.what()<<std::endl;
            std::string errStr = ss.str();
            cstr = errStr.c_str();
        }

        return cstr;
    }

const char* SetGlobalChainParams(const char* networkStr) {
        const char* cstr = nullptr;

        try {
            const std::string network(networkStr);
            SelectParams(network); // throw exception if wrong network string
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ << " "<< e.what()<<std::endl;
            std::string errStr = ss.str();
            cstr = errStr.c_str();
        }

        return cstr;
    }