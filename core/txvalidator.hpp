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
#include <validatearg.hpp>
#include <txerror.h>
#include <doserror.hpp>
#include <extendedTx.hpp>

namespace bsv
{

/**
 * CTxValidator holds its own ConfigScriptPolicy, ChainParams and CCancellationSource
 * objects in order to execute the script fully. It forwards the node's settings to its
 * own ConfigScriptPolicy instance through all setters.
 *
 * Checks present in bitcoin-sv's TxnValidation / BlockValidateTxns that are intentionally
 * absent here, with guidance for node implementors who need them:
 *
 *   Promiscuous mempool script flags:
 *     bitcoin-sv's policy path can replace standard script verify flags with an
 *     operator-configured set via -promiscuousmempool. The new node architecture has
 *     no mempool, so this operator feature does not exist and is intentionally absent.
 *
 *   nLockTime finality (IsFinalTx):
 *     bitcoin-sv rejects or queues transactions whose nLockTime has not yet been reached,
 *     using the chain tip's Median Time Past (MTP). BDK has no access to MTP or chain state.
 *     Policy path: IsFinalTx(tx, tipHeight + 1, tipMedianTimePast).
 *     Consensus path: IsFinalTx(tx, blockHeight, previousBlockMedianTimePast).
 *
 *   BIP68 sequence locks (CheckSequenceLocks / SequenceLocks, pre-Genesis only):
 *     bitcoin-sv enforces relative time locks from BIP68 using per-input UTXO confirmation
 *     heights and MTP. BDK has UTXO heights available but not MTP. A node can implement
 *     this by calling CalculateSequenceLocks / EvaluateSequenceLocks with the required
 *     chain context before or after CheckTransaction.
 *
 *   Non-mandatory script flag retry:
 *     bitcoin-sv retries a failed script without StandardNotMandatoryScriptVerifyFlags and
 *     downgrades the failure to DoS 0 / REJECT_NONSTANDARD when only a non-mandatory flag
 *     caused the rejection. BDK exposes VerifyScript with custom flags; a node can perform
 *     this retry itself and apply whatever DoS classification it chooses.
 *
 *   Grace period inverse flag retry (Genesis / Chronicle activation windows):
 *     bitcoin-sv retries with inverted per-protocol input flags during activation grace
 *     periods and returns a soft "genesis-script-verify-flag-failed" or
 *     "chronicle-script-verify-flag-failed" error on success. A node can implement this
 *     using VerifyScript with custom flags and InProtocolGracePeriod / GetInverseProtocolEra.
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
        // SetMaxSigOpsPostGenesisPolicy mirrors bitcoin-sv SetMaxTxSigOpsCountPolicy semantics:
        //   0 → reset to default (UINT32_MAX); negative or > UINT32_MAX → rejected with error.
        void SetMaxSigOpsPolicy(uint64_t value);
        uint64_t GetMaxSigOpsPolicy() const;
        bool SetMaxSigOpsPostGenesisPolicy(int64_t value, std::string* err = nullptr);
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
        //   pass (blockFlags & SCRIPT_VERIFY_P2SH) != 0 for block-level aggregate counting
        //   (blockFlags from GetBlockScriptFlags); pass true for policy/mempool use.
        // consensus controls the era used to determine the sigops rules:
        //   false (policy) → era at blockHeight+1 (next candidate block)
        //   true (block)   → era at blockHeight (the block being validated)
        // Callers must not pass MEMPOOL_HEIGHT utxo heights when consensus=true; this method
        // throws std::runtime_error on that sentinel value. ValidateTransaction guards this via
        // UnconfirmedInputInBlock before reaching sigop counting, so that path is safe; direct
        // callers are responsible for filtering out unconfirmed UTXOs beforehand.
        // It might throw an exception if any issue to calculate the number of sigops.
        uint64_t GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool countP2SHSigOps, bool consensus = false) const;

        // CalculateFlags computes the script verify flags for a single input.
        // consensus controls which protocol era and flag set are used (block vs policy).
        // Note: if utxoHeight == MEMPOOL_HEIGHT and consensus=true, the UTXO era is silently
        // mapped to blockHeight+1 rather than returning an error, because this helper returns
        // uint32_t and cannot signal a typed failure. ValidateTransaction rejects this case with
        // UnconfirmedInputInBlock before CalculateFlags is ever reached; callers invoking
        // CalculateFlags directly with consensus=true and MEMPOOL_HEIGHT are responsible for
        // guarding against this invalid combination beforehand.
        //   - consensus=false --> flags to check a tx coming from a peer  (policy path:    enforce policy checks)
        //   - consensus=true  --> flags to check a tx coming from a block (consensus path: skip   policy checks)
        uint32_t CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const;

        // VerifyScript extract the extended transaction, then forward to bsv call
        //
        // If client uses a custom flags different than zero, then this will be used
        // instead of the implicitly calculated flags
        TxError VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags = std::span<const uint32_t>()) const;

        // ValidateBatch processes multiple transaction validations in a batch.
        // Returns a vector of TxError results, one for each ValidateArg in the input.
        //
        // NOT THREAD-SAFE: The caller must build the batch and call ValidateBatch on
        // the same thread. ValidateArg entries hold non-owning spans into caller memory;
        // concurrent access from another thread is a data race.
        std::vector<TxError> ValidateBatch(const ValidateBatch& batch) const;

        // ValidateTransaction runs all tx-level checks then script verification.
        // consensus=false → peer/mempool context (all checks including policy)
        // consensus=true  → block context (consensus checks only)
        //
        // This method is for non-coinbase transactions only. Coinbase transactions
        // are rejected with CoinbaseNotAllowed in both modes. In block validation,
        // the node must validate the coinbase transaction separately (bitcoin-sv uses
        // CheckCoinbase); ValidateTransaction should be called only for the remaining
        // non-coinbase transactions in the block.
        TxError ValidateTransaction(std::span<const uint8_t> extendedTX,
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