#include <iostream>
#include <string>

#include <config.h>
#include <cgo/global_scriptconfig.h>


const char* SetGlobalScriptConfig(
        const char* chainNetwork,
        unsigned long long maxOpsPerScriptPolicyIn,
        unsigned long long maxScriptNumLengthPolicyIn,
        unsigned long long maxScriptSizePolicyIn,
        unsigned long long maxPubKeysPerMultiSigIn,
        unsigned long long maxStackMemoryUsageConsensusIn,
        unsigned long long maxStackMemoryUsagePolicyIn
    ) {
        char* errStr = nullptr;

        try {
            std::string err;
            auto& gConfig = GlobalConfig::GetModifiableGlobalConfig();

            // Set the Chain Params and the genesis/chronicle height in config.
            // We have to set these heights because they are used inside the config
            // not inside the ChainParams
            const std::string network(chainNetwork);
            SelectParams(network); // throw exception if wrong network string
            const CChainParams& chainparams = gConfig.GetChainParams(); // ChainParams after setting the chain network
            if (!gConfig.SetGenesisActivationHeight(chainparams.GetConsensus().genesisHeight, &err)) {
                throw std::runtime_error("unable to set genesis activation height : " + err);
            }
            if (!gConfig.SetChronicleActivationHeight(chainparams.GetConsensus().chronicleHeight, &err)) {
                throw std::runtime_error("unable to set chronicle activation height : " + err);
            }

            bool ok=true;
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

            errStr = new char[err.size() + 1];// +1 for the null terminator
            std::strcpy(errStr, err.c_str());
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ << " "<< e.what()<<std::endl;
            std::string expStr = ss.str();
            errStr = new char[expStr.size() + 1];
            std::strcpy(errStr, expStr.c_str());  // Copy the string data
        }

        return errStr;
    }