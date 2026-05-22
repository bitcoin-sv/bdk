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
 * absent here. In every case BDK is missing the chain / mempool / node-state context the
 * check needs, so the work is delegated to the node implementation. Brief guidance:
 *
 *   Promiscuous mempool script flags (-promiscuousmempoolflags):
 *     Operator option in bitcoin-sv that replaces standard script-verify flags with a
 *     custom set at the mempool layer. BDK has no mempool concept; intentionally absent.
 *
 *   nLockTime finality (IsFinalTx):
 *     Requires chain-tip MTP, candidate block header time, or candidate-parent MTP
 *     depending on validation path. BDK has no chain-tip, block-time, or MTP state. The
 *     node implementation must compute finality itself before or after CheckTransaction.
 *     Source of truth per context:
 *       policy / next-tip:     IsFinalTx(tx, tipHeight + 1, tipMedianTimePast).
 *       block-validation:      IsFinalTx(tx, blockHeight, candidateParentMedianTimePast)
 *                              at/after CSVHeight, candidate block time before CSV.
 *
 *   BIP68 sequence locks (CheckSequenceLocks / SequenceLocks, pre-Genesis only):
 *     Needs per-input UTXO MTP from chain state. BDK has UTXO heights but not MTP. The
 *     node implementation should call CalculateSequenceLocks / EvaluateSequenceLocks with
 *     the required chain context before or after CheckTransaction.
 *
 *   Non-mandatory script flag retry (DoS-0 downgrade):
 *     bitcoin-sv re-runs a failed script without StandardNotMandatoryScriptVerifyFlags and
 *     downgrades the failure to DoS 0 / REJECT_NONSTANDARD when only a non-mandatory flag
 *     caused the rejection. BDK does not classify failures by peer-DoS impact; it exposes
 *     VerifyScript with custom flags so the node implementation can perform the retry and
 *     apply its own DoS classification.
 *
 *   Grace-period retries (Genesis / Chronicle activation windows):
 *     bitcoin-sv has two distinct retries inside the activation window, both intentionally
 *     left to the node implementation:
 *       (a) per-input inverse-flag retry inside CheckInputs, returning a soft
 *           "genesis-script-verify-flag-failed" or "chronicle-script-verify-flag-failed"
 *           error on success;
 *       (b) whole-transaction re-check with the previous-era rules inside TxnValidation,
 *           returning errors prefixed "flexible-..." at DoS 0 when only the previous-era
 *           rules pass.
 *     BDK exposes VerifyScript with custom flags and grace-period settings, so the
 *     node implementation can compute the era / grace-window logic and build both paths.
 *
 * Fee-check limitations vs bitcoin-sv:
 *
 *   BDK's static-fee-path floor (implCheckFee, invoked from ValidateTransaction in policy
 *   mode) is a faithful port of bitcoin-sv's TxnValidation fee check under the simplifying
 *   assumption that the caller has no mempool state. The following bitcoin-sv mempool-fee
 *   features are NOT reproduced here, because BDK has no mempool to read from:
 *
 *     - Dynamic eviction-driven reject fee:
 *         bitcoin-sv's mempoolRejectFee = pool.GetMinFee(maxmempool) can rise above the
 *         static -minminingtxfee rate under eviction pressure. BDK uses a single static
 *         rate (SetMinMiningTxFee) for both the floor and the implied minimum credit, so
 *         under mempool pressure bitcoin-sv may reject txs BDK accepts.
 *
 *     - Separate blockMinTxFee vs mempoolRejectFee quantities:
 *         bitcoin-sv tracks them independently. BDK collapses to one. Behaviourally
 *         equivalent only when the two are equal (the static case).
 *
 *     - PrioritiseTransaction operator API:
 *         bitcoin-sv supports per-tx fee deltas injected by the operator. BDK has no
 *         per-tx delta state and no equivalent API.
 *
 *   For free-consolidation classification itself (the gate that bypasses the floor),
 *   BDK runs the same rules as bitcoin-sv IsFreeConsolidationTxn — see implIsFreeConsolidation.
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

        // Consolidation policy settings — signatures mirror bitcoin-sv's
        // GlobalConfig setters (signed input, error on negative). Zero handling
        // also mirrors bitcoin-sv:
        //   SetMinConsolidationFactor:        0 stored literally (disables consolidation entirely)
        //   SetMaxConsolidationInputScriptSize: 0 → reset to default (150)
        //   SetMinConfConsolidationInput:      0 → reset to default (6)
        bool SetMinConsolidationFactor(int64_t value, std::string* err = nullptr);
        bool SetMaxConsolidationInputScriptSize(int64_t value, std::string* err = nullptr);
        bool SetMinConfConsolidationInput(int64_t value, std::string* err = nullptr);
        void SetAcceptNonStdConsolidationInput(bool value);

        // Static-fee-path policy setting. Stored as integer satoshis/kB to match
        // bitcoin-sv CFeeRate::GetFee bitwise. 0 means "no fee policy" (every tx
        // passes the fee floor); negative is rejected via the error channel.
        bool SetMinMiningTxFee(int64_t satoshisPerKB, std::string* err = nullptr);
        int64_t GetMinMiningTxFee() const;

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

        // Consolidation policy settings (not part of ConfigScriptPolicy).
        // Defaults mirror bitcoin-sv DEFAULT_MIN_CONSOLIDATION_FACTOR (20),
        // DEFAULT_MAX_CONSOLIDATION_INPUT_SCRIPT_SIZE (150),
        // DEFAULT_MIN_CONF_CONSOLIDATION_INPUT (6), DEFAULT_ACCEPT_NON_STD_CONSOLIDATION_INPUT (false).
        uint64_t consolidationMinFactor{20};
        uint64_t consolidationMaxInputScriptSize{150};
        uint64_t consolidationMinConf{6};
        bool consolidationAcceptNonStd{false};

        // Static-fee-path policy setting (not part of ConfigScriptPolicy).
        // 0 means "no fee policy" — every tx passes the fee floor.
        int64_t minMiningTxFeeSatPerKB{0};

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

        TxError implCheckFee(
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