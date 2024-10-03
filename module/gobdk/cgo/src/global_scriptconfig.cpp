#include <iostream>
#include <string>

#include <config.h>
#include <cgo/global_scriptconfig.h>
#include <core/interpreter_bdk.hpp>


const char* CgoSetGlobalScriptConfig(
        const char* chainNetwork,
        int64_t maxOpsPerScriptPolicyIn,
        int64_t maxScriptNumLengthPolicyIn,
        int64_t maxScriptSizePolicyIn,
        int64_t maxPubKeysPerMultiSigIn,
        int64_t maxStackMemoryUsageConsensusIn,
        int64_t maxStackMemoryUsagePolicyIn
    ) {
        std::string err;
        try {
            std::string network(chainNetwork);
            err = bsv::SetGlobalScriptConfig(
                network,
                maxOpsPerScriptPolicyIn,
                maxScriptNumLengthPolicyIn,
                maxScriptSizePolicyIn,
                maxPubKeysPerMultiSigIn,
                maxStackMemoryUsageConsensusIn,
                maxStackMemoryUsagePolicyIn
            );
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ << " "<< e.what()<<std::endl;
            err = ss.str();
        }

        if (!err.empty()) {
            char* errStr = new char[err.size() + 1];
            std::strcpy(errStr, err.c_str());  // Copy the string data
            return errStr;
        }

        return nullptr;
    }