#ifndef __SCRIPTENGINE_HPP__
#define __SCRIPTENGINE_HPP__

#include <string>
#include <cstdint>
#include <span>
#include <memory>
#include <optional>
#include <utility>

#include <config.h>
#include <chainparams.h>
#include <taskcancellation.h>
#include <script/script.h>
#include <script/malleability_status.h>
#include <script/interpreter.h>

namespace bsv
{
/**
 *
 * CScriptEngine hold its own GlobalConfig, ChainParams and CCancellationSource
 * objects in order to execute the script fully
 *
 * It forward the config settings to its own GlobalConfig instance
 *
 * TODO :
 *   - Move this class to core part and use it for all other languages binding
 *   - Remove the usage of old implementation where it use implicitly global objects in bsv
 *
 */
class CScriptEngine {
    public:
        CScriptEngine(const std::string chainName);

        // Forward policy settings call to GlobalConfig
        bool SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* error);
        bool SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err);
        bool SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err);
        bool SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err);
        bool SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err);

        // Forward set genesis height and chronicle height to GlobalConfig
        bool SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err);
        bool SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err);

        // Forward getter to GlobalConfig call
        uint64_t GetMaxOpsPerScript(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxScriptNumLength(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const;// Genesis and Chronicle should not both true
        uint64_t GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const;
        int32_t GetGenesisActivationHeight() const;
        int32_t GetChronicleActivationHeight() const;

        // GetSigOpCount returns the number of sig ops in a extended transactions
        // It might throw an exception if any issue to calculate the number of sigops
        uint64_t GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight) const;

        // CalculateFlags forward to bitcoin-sv call
        // consensus define what flags set are being used
        //   - consensus=true  --> flags to check a tx coming from a peer  (enforce policies check)
        //   - consensus=false --> flags to check a tx coming from a block (   skip policies check)
        uint32_t CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const;

        // VerifyScript extract the extended transaction, then forward to bsv call
        ScriptError VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus) const;

    private :
        GlobalConfig bsvConfig;
        std::unique_ptr<CChainParams> chainParams;
        std::shared_ptr<task::CCancellationSource> source;

        ScriptError verifyImpl(
            const CScript& unlocking_script,
            const CScript& locking_script,
            const bool consensus,
            const unsigned int flags,
            BaseSignatureChecker& sig_checker,
            std::atomic<malleability::status>& malleability
        ) const;

        // helper to convert the optional (returned by ScriptVerify) to raw ScriptError
        ScriptError helperOptional2ScriptError(const std::optional<std::pair<bool, ScriptError>>& ret) const;
};

// export the value of SCRIPT_ERR_ERROR_COUNT
int CPP_SCRIPT_ERR_ERROR_COUNT();

} // namespace bsv

#endif /* __SCRIPTENGINE_HPP__ */