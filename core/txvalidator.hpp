#ifndef __TXVALIDATOR_HPP__
#define __TXVALIDATOR_HPP__

#include <string>
#include <cstdint>
#include <span>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <configscriptpolicy.h>
#include <chainparams.h>
#include <taskcancellation.h>
#include <script/script.h>
#include <script/interpreter.h>
#include <verifyarg.hpp>
#include <txerror.h>
#include <doserror.hpp>
#include <extendedTx.hpp>

namespace bsv
{

/**
 *
 * CTxValidator hold its own GlobalConfig, ChainParams and CCancellationSource
 * objects in order to execute the script fully
 *
 * It forward the config settings to its own GlobalConfig instance
 *
 * TODO :
 *   - Move this class to core part and use it for all other languages binding
 *   - Remove the usage of old implementation where it use implicitly global objects in bsv
 *
 */
class CTxValidator {
    public:
        CTxValidator(const std::string chainName);

        // Forward policy settings call to GlobalConfig
        bool SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* error);
        bool SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err);
        bool SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err);
        bool SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err);
        bool SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err);

        // Forward set genesis height and chronicle height to GlobalConfig
        bool SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err);
        bool SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err);
        bool SetGenesisGracefulPeriod(int64_t genesisGracefulPeriodIn, std::string* err);
        bool SetChronicleGracefulPeriod(int64_t chronicleGracefulPeriodIn, std::string* err);

        // Forward other policy setters to ConfigScriptPolicy
        bool SetMaxTxSizePolicy(int64_t value, std::string* err);
        void SetDataCarrierSize(uint64_t dataCarrierSize);
        void SetDataCarrier(bool dataCarrier);
        void SetAcceptNonStandardOutput(bool accept);
        void SetRequireStandard(bool require);
        void SetPermitBareMultisig(bool permit);

        // Consolidation policy settings
        void SetMinConsolidationFactor(uint64_t value);
        void SetMaxConsolidationInputScriptSize(uint64_t value);
        void SetMinConfConsolidationInput(uint64_t value);
        void SetAcceptNonStdConsolidationInput(bool value);

        // SigOps policy limits. Pre-Genesis default: 4000 (MAX_TX_SIGOPS_COUNT_POLICY_BEFORE_GENESIS).
        // Post-Genesis default: UINT32_MAX (MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS); operators can lower it.
        void SetMaxSigOpsPolicy(uint64_t value);
        uint64_t GetMaxSigOpsPolicy() const;
        void SetMaxSigOpsPostGenesisPolicy(uint64_t value);
        uint64_t GetMaxSigOpsPostGenesisPolicy() const;

        // Reset all policy settings to defaults
        void ResetDefault();

        // Forward getter to GlobalConfig call
        uint64_t GetMaxOpsPerScript(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxScriptNumLength(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const;// Genesis and Chronicle should not both true
        uint64_t GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const;
        uint64_t GetMaxTxSize(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const; // Genesis and Chronicle should not both true
        uint64_t GetDataCarrierSize() const;
        bool GetDataCarrier() const;
        bool GetAcceptNonStandardOutput(bool isGenesisEnabled, bool isChronicleEnabled) const; // Genesis and Chronicle should not both true
        bool GetRequireStandard() const;
        bool GetPermitBareMultisig() const;
        int32_t GetGenesisActivationHeight() const;
        int32_t GetChronicleActivationHeight() const;
        uint64_t GetGenesisGracefulPeriod() const;
        uint64_t GetChronicleGracefulPeriod() const;

        // Consolidation policy getters
        uint64_t GetMinConsolidationFactor() const;
        uint64_t GetMaxConsolidationInputScriptSize() const;
        uint64_t GetMinConfConsolidationInput() const;
        bool GetAcceptNonStdConsolidationInput() const;

        // GetSigOpCount returns the number of sig ops in an extended transaction.
        // countP2SHSigOps mirrors the fP2SH parameter in bitcoin-sv's GetTransactionSigOpCount:
        // the node should pass (blockFlags & SCRIPT_VERIFY_P2SH) != 0, where blockFlags comes
        // from GetBlockScriptFlags, so that P2SH sigops are only counted when the block enforces P2SH.
        // It might throw an exception if any issue to calculate the number of sigops.
        uint64_t GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool countP2SHSigOps) const;

        // CalculateFlags forward to bitcoin-sv call
        // consensus define what flags set are being used
        //   - consensus=true  --> flags to check a tx coming from a peer  (enforce policies check)
        //   - consensus=false --> flags to check a tx coming from a block (   skip policies check)
        uint32_t CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const;

        // VerifyScript extract the extended transaction, then forward to bsv call
        //
        // If client uses a custom flags different than zero, then this will be used
        // instead of the implicitly calculated flags
        TxError VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags = std::span<const uint32_t>()) const;

        // VerifyScriptBatch processes multiple script verifications in a batch
        // Returns a vector of TxError results, one for each VerifyArg in the input
        std::vector<TxError> VerifyScriptBatch(const VerifyBatch& batch) const;

        // CheckTransaction runs all tx-level checks then script verification.
        // consensus=false → peer context (all checks including policy)
        // consensus=true  → block context (consensus checks only)
        TxError CheckTransaction(std::span<const uint8_t> extendedTX,
                                 std::span<const int32_t> utxoHeights,
                                 int32_t blockHeight,
                                 bool consensus) const;

        // CheckStandardness verifies IsStandardTx + AreInputsStandard.
        // Policy-path check only (not applicable in block/consensus context).
        TxError CheckStandardness(std::span<const uint8_t> extendedTX,
                                  std::span<const int32_t> utxoHeights,
                                  int32_t blockHeight) const;

        // CheckPrevOutputs rejects any input with a null prevout (all-zero txid + 0xFFFFFFFF index).
        // Both peer and block context.
        TxError CheckPrevOutputs(std::span<const uint8_t> extendedTX) const;

        // CheckOutputs rejects P2SH locking scripts in outputs after Genesis activation height.
        // Both peer and block context.
        TxError CheckOutputs(std::span<const uint8_t> extendedTX, int32_t blockHeight) const;

        // CheckConsensusSigops enforces the pre-Genesis 20,000 sigop limit (inputs + outputs + P2SH redeem scripts).
        // Block context. Mirrors BlockValidateTxns in bitcoin-sv validation.cpp.
        TxError CheckConsensusSigops(std::span<const uint8_t> extendedTX,
                                     std::span<const int32_t> utxoHeights,
                                     int32_t blockHeight) const;

        // CheckSigOpsPolicy enforces the configurable sigop policy limit (with P2SH redeem scripts).
        // Peer/mempool context only.
        TxError CheckSigOpsPolicy(std::span<const uint8_t> extendedTX,
                                  std::span<const int32_t> utxoHeights,
                                  int32_t blockHeight) const;

        // IsFreeConsolidation returns OK if the transaction qualifies as a fee-exempt consolidation.
        // Returns NotFreeConsolidation if it does not qualify.
        // Peer/mempool context only.
        TxError IsFreeConsolidation(std::span<const uint8_t> extendedTX,
                                    std::span<const int32_t> utxoHeights,
                                    int32_t blockHeight) const;

    private :
        ConfigScriptPolicy policySettings;
        std::unique_ptr<CChainParams> chainParams;
        std::shared_ptr<task::CCancellationSource> source;

        // Consolidation policy settings (not part of ConfigScriptPolicy)
        uint64_t consolidationMinFactor{20};
        uint64_t consolidationMaxInputScriptSize{150};
        uint64_t consolidationMinConf{6};
        bool consolidationAcceptNonStd{false};

        // SigOps policy limits (not part of ConfigScriptPolicy)
        // Pre-Genesis default: 4000 = MAX_TX_SIGOPS_COUNT_POLICY_BEFORE_GENESIS (20000/5)
        // Post-Genesis default: UINT32_MAX = MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS
        uint64_t maxSigOpsPolicy{4000};
        uint64_t maxSigOpsPostGenesisPolicy{UINT32_MAX};

        // Per-input script execution — thin wrapper around the BSV core ::VerifyScript.
        TxError bsvVerifyScript(
            const CScript& unlocking_script,
            const CScript& locking_script,
            const bool consensus,
            const unsigned int flags,
            BaseSignatureChecker& sig_checker
        ) const;

        TxError implCheckInputValues(
            const CTransaction& tx,
            const std::vector<CTxOut>& prevUTXO
        ) const;

        // Whole-tx script verification loop (no deserialization; can throw).
        TxError implVerifyScript(
            const CTransaction& ctx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            int32_t blockHeight,
            bool consensus,
            std::span<const uint32_t> customFlags = std::span<const uint32_t>()
        ) const;

        TxError implCheckStandardness(
            const CTransaction& tx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            int32_t blockHeight
        ) const;

        TxError implCheckTransactionCommon(
            const CTransaction& tx,
            int32_t blockHeight
        ) const;

        TxError implCheckPrevOutputs(
            const CTransaction& tx
        ) const;

        TxError implCheckOutputs(
            const CTransaction& tx,
            int32_t blockHeight
        ) const;

        TxError implCheckConsensusSigops(
            const CTransaction& tx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            int32_t blockHeight
        ) const;

        TxError implCheckSigOpsPolicy(
            const CTransaction& tx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            int32_t blockHeight
        ) const;

        TxError implIsFreeConsolidation(
            const CTransaction& tx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            int32_t blockHeight
        ) const;

        uint64_t implGetSigOpCount(
            const CTransaction& ctx,
            const std::vector<CTxOut>& prevUTXO,
            std::span<const int32_t> utxoHeights,
            ProtocolEra era,
            int32_t nextBlockHeight,
            bool countP2SHSigOps
        ) const;
};

// export the value of SCRIPT_ERR_ERROR_COUNT
int CPP_SCRIPT_ERR_ERROR_COUNT();

} // namespace bsv

#endif /* __TXVALIDATOR_HPP__ */