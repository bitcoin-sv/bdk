#ifndef __INTERPRETER_BDK_HPP__
#define __INTERPRETER_BDK_HPP__



#include <script/script_error.h>

#include <span>
#include <string>
#include <vector>
#include <cstdint>

namespace bsv
{

    /**
     * Set ScriptConfig globally for the core C++. It return the error string if any
     */
    std::string SetGlobalScriptConfig(
        std::string chainNetwork,
        int64_t maxOpsPerScriptPolicy,
        int64_t maxScriptNumLengthPolicy,
        int64_t maxScriptSizePolicy,
        int64_t maxPubKeysPerMultiSig,
        int64_t maxStackMemoryUsageConsensus,
        int64_t maxStackMemoryUsagePolicy,
        int32_t customGenesisHeight = 0
    );

    int32_t GetGenesisActivationHeight();

    // script_verification_flags calculates the flags to be used when verifying scripts
    uint32_t script_verification_flags(const std::span<const uint8_t> locking_script, const bool isPostChronical);
    uint32_t script_verification_flags_v2(const std::span<const uint8_t> locking_script, int32_t blockHeight);

    ScriptError execute(std::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(std::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags,
                        std::span<const uint8_t> tx,
                        int _index,
                        int64_t amount);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags,
                        const std::string& tx,
                        int index,
                        int64_t amount);

    ScriptError verify(std::span<const uint8_t> unlocking_script,
                       std::span<const uint8_t> locking_script,
                       bool consensus,
                       unsigned int flags,
                       std::span<const uint8_t> tx,
                       int index,
                       int64_t amount);

    // Verify extended tx in one go. It will iterate through all the input and verify
    // It returns the first encountered verification error
    ScriptError verify_extend(std::span<const uint8_t> extendedTX, int32_t blockHeight, bool consensus);
};

#endif /* __INTERPRETER_BDK_HPP__ */
