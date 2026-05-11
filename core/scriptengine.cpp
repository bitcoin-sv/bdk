#include <base58.h>
#include <core_io.h>
#include <protocol_era.h>
#include <policy/policy.h>
#include <verify_script_flags.h>
#include <script/script_flags.h>
#include <script/standard.h>

#include <chainparams_bdk.hpp>
#include <checker_cache.hpp>
#include <extendedTx.hpp>
#include <scriptengine.hpp>

bsv::CScriptEngine::CScriptEngine(const std::string chainName)
    : chainParams{ std::move(bsv::CreateCustomChainParams(chainName)) }
    , source{ task::CCancellationSource::Make() }
{
    std::string errStr;
    bool ok {true};

    policySettings.SetRequireStandard(chainParams->RequireStandard());

    int32_t genesisHeight;
    int32_t chronicleHeight;
    ok = ok && this->SetGenesisActivationHeight(chainParams->GetConsensus().genesisHeight, &errStr);
    ok = ok && this->SetChronicleActivationHeight(chainParams->GetConsensus().chronicleHeight, &errStr);
    if (!(ok || errStr.empty())){
        throw std::runtime_error("error setting genesis and chronicle heights " + errStr);
    }
}

bool bsv::CScriptEngine::SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* err)
{
    return policySettings.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, err);
}

bool bsv::CScriptEngine::SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err)
{
    return policySettings.SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, err);
}

bool bsv::CScriptEngine::SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err)
{
    return policySettings.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, err);
}

bool bsv::CScriptEngine::SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err)
{
    return policySettings.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, err);
}

bool bsv::CScriptEngine::SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err)
{
    return policySettings.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, err);
}

bool bsv::CScriptEngine::SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && policySettings.SetGenesisActivationHeight(genesisActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto chronicleHeight = policySettings.GetChronicleActivationHeight();
    if ( chronicleHeight < policySettings.GetGenesisActivationHeight() ) {
        ok = ok && policySettings.SetChronicleActivationHeight(genesisActivationHeightIn, err);
    }

    return ok;
}

bool bsv::CScriptEngine::SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && policySettings.SetChronicleActivationHeight(chronicleActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto genesisHeight = policySettings.GetGenesisActivationHeight();
    if ( policySettings.GetChronicleActivationHeight() < genesisHeight ) {
        ok = ok && policySettings.SetGenesisActivationHeight(chronicleActivationHeightIn, err);
    }

    return ok;
}

uint64_t bsv::CScriptEngine::GetMaxOpsPerScript(bool isGenesisEnabled, bool consensus) const
{
    return policySettings.GetMaxOpsPerScript(isGenesisEnabled, consensus);
}

uint64_t bsv::CScriptEngine::GetMaxScriptNumLength(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era {ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }

    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetMaxScriptNumLength(era, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t bsv::CScriptEngine::GetGenesisActivationHeight() const
{
    return policySettings.GetGenesisActivationHeight();
}

int32_t bsv::CScriptEngine::GetChronicleActivationHeight() const
{
    return policySettings.GetChronicleActivationHeight();
}

uint64_t bsv::CScriptEngine::GetGenesisGracefulPeriod() const
{
    return policySettings.GetGenesisGracefulPeriod();
}

uint64_t bsv::CScriptEngine::GetChronicleGracefulPeriod() const
{
    return policySettings.GetChronicleGracefulPeriod();
}

bool bsv::CScriptEngine::SetGenesisGracefulPeriod(int64_t genesisGracefulPeriodIn, std::string* err)
{
    return policySettings.SetGenesisGracefulPeriod(genesisGracefulPeriodIn, err);
}

bool bsv::CScriptEngine::SetChronicleGracefulPeriod(int64_t chronicleGracefulPeriodIn, std::string* err)
{
    return policySettings.SetChronicleGracefulPeriod(chronicleGracefulPeriodIn, err);
}

bool bsv::CScriptEngine::SetMaxTxSizePolicy(int64_t value, std::string* err)
{
    return policySettings.SetMaxTxSizePolicy(value, err);
}

void bsv::CScriptEngine::SetDataCarrierSize(uint64_t dataCarrierSize)
{
    policySettings.SetDataCarrierSize(dataCarrierSize);
}

void bsv::CScriptEngine::SetDataCarrier(bool dataCarrier)
{
    policySettings.SetDataCarrier(dataCarrier);
}

void bsv::CScriptEngine::SetAcceptNonStandardOutput(bool accept)
{
    policySettings.SetAcceptNonStandardOutput(accept);
}

void bsv::CScriptEngine::SetRequireStandard(bool require)
{
    policySettings.SetRequireStandard(require);
}

void bsv::CScriptEngine::SetPermitBareMultisig(bool permit)
{
    policySettings.SetPermitBareMultisig(permit);
}

void bsv::CScriptEngine::ResetDefault()
{
    policySettings.ResetDefault();
}

uint64_t bsv::CScriptEngine::GetMaxTxSize(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era{ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }
    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetMaxTxSize(era, isConsensus);
}

uint64_t bsv::CScriptEngine::GetDataCarrierSize() const
{
    return policySettings.GetDataCarrierSize();
}

bool bsv::CScriptEngine::GetDataCarrier() const
{
    return policySettings.GetDataCarrier();
}

bool bsv::CScriptEngine::GetAcceptNonStandardOutput(bool isGenesisEnabled, bool isChronicleEnabled) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era{ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }
    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetAcceptNonStandardOutput(era);
}

bool bsv::CScriptEngine::GetRequireStandard() const
{
    return policySettings.GetRequireStandard();
}

bool bsv::CScriptEngine::GetPermitBareMultisig() const
{
    return policySettings.GetPermitBareMultisig();
}

uint32_t bsv::CScriptEngine::CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const
{
    ProtocolEra era;
    uint32_t protocolFlags;
    if (consensus) {
        // For tx coming from a block.
        // blockHeight is the block the tx belong to
        // In that case, the chainTip is blockHeight-1
        era = GetProtocolEra(policySettings, blockHeight );
        const Consensus::Params& consensusparams = chainParams->GetConsensus();
        protocolFlags = GetBlockScriptFlags(consensusparams, blockHeight - 1 , era);
    } else {
        // For tx coming from a peer
        // blockHeight is the chain tip (highest current block)
        era = GetProtocolEra(policySettings, blockHeight + 1);
        const bool requireStandard = chainParams->RequireStandard();
        protocolFlags = GetScriptVerifyFlags(era, requireStandard);
    }

    ProtocolEra utxoEra{ GetProtocolEra(policySettings, utxoHeight) };
    const uint32_t utxoFlags { InputScriptVerifyFlags(era, utxoEra) };

    return (protocolFlags | utxoFlags);
}

// The implementation of this method replicate the bsv method ::GetTransactionSigOpCount
// where inside it has GetSigOpCountWithoutP2SH and GetP2SHSigOpCount
// These methods are all implemented in src/validation.cpp that we can not have it because
// of the huge dependencies
//
// So we replicate them here
uint64_t bsv::CScriptEngine::GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight) const {
    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty()) {
        throw std::runtime_error("error serializing extended tx");
    }

    const CTransaction ctx(eTX.mtx);

    // SigOpCount is use only in case the tx come from a peer
    // Where the protocol era is calculated based on blockHeight + 1
    const auto era = GetProtocolEra(policySettings, blockHeight + 1);


    // The part GetSigOpCountWithoutP2SH //////////////////////////////////////////////////
    bool sigOpCountError = false;
    uint64_t nSigOpsWithoutP2SH {0};
    for (const auto& txin : ctx.vin)
    {
        // After Genesis, this should return 0, since only push data is allowed in input scripts:
        nSigOpsWithoutP2SH += txin.scriptSig.GetSigOpCount(false, era, sigOpCountError);
        if (sigOpCountError) {
            throw std::runtime_error("error calculating sigop count without P2SH for scriptSigs");
        }
    }
    for (const auto& txout : ctx.vout)
    {
        nSigOpsWithoutP2SH += txout.scriptPubKey.GetSigOpCount(false, era, sigOpCountError);
        if (sigOpCountError) {
            throw std::runtime_error("error calculating sigop count without P2SH for scriptPubKeys");
        }
    }

    if (ctx.IsCoinBase()) {
        return nSigOpsWithoutP2SH;
    }

    // The part GetP2SHSigOpCount //////////////////////////////////////////////////
    if (eTX.vutxo.size() != ctx.vin.size()) {
        throw std::runtime_error("inconsistent inputs size");
    }
    if (eTX.vutxo.size() != utxoHeights.size()) {
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    }

    uint64_t nSigOpsP2SH{0};
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const ProtocolEra utxoEra = GetProtocolEra(policySettings, utxoHeights[index]);
        if (IsProtocolActive(utxoEra, ProtocolName::Genesis)) {
            continue;
        }

        const CTxOut& prevout = eTX.vutxo[index];
        const CTxIn& input = ctx.vin[index];
        if (IsP2SH(prevout.scriptPubKey)) {
            nSigOpsP2SH += prevout.scriptPubKey.GetSigOpCount(input.scriptSig, utxoEra, sigOpCountError);
            if (sigOpCountError) {
                throw std::runtime_error("error calculating P2SH sigop count");
            }
        }
    }

    return (nSigOpsWithoutP2SH + nSigOpsP2SH);
}

// Helper function to perform standardness checks
// Returns SCRIPT_ERR_OK if all checks pass, otherwise returns SCRIPT_ERR_UNKNOWN_ERROR
// Replicates the standardness checks performed by bitcoin-sv's TxnValidation
// (validation.cpp) when a transaction is received from a peer (consensus=false).
// This is never called during block validation (consensus=true).
//
// The check has two stages, both governed by acceptNonStandardOutput, which is
// era-dependent:
//   pre-Genesis  : acceptNonStandardOutput = !requireStandard
//   post-Genesis : acceptNonStandardOutput = policySettings.acceptNonStandardOutput (default true)
//
// Stage 1 — output standardness (IsStandardTx):
//   FAILS when IsStandardTx returns false AND:
//     - acceptNonStandardOutput is false, OR
//     - era is Genesis AND requireStandard is true AND reason != "scriptpubkey"
//   PASSES otherwise. In particular, post-Genesis with acceptNonStandardOutput=true
//   (mainnet default), a non-standard output script of type "scriptpubkey" (any
//   unrecognised script) is explicitly allowed.
//
// Stage 2 — input standardness (AreInputsStandard equivalent):
//   Only executed when acceptNonStandardOutput is false.
//   FAILS when any input's scriptSig is not standard for the UTXO's era.
//   PASSES (skipped entirely) when acceptNonStandardOutput is true.
//
// Summary of common mainnet scenarios (post-Genesis, requireStandard=true,
// acceptNonStandardOutput=true by default):
//   - Standard tx                                       → PASSES (stage 1 ok, stage 2 skipped)
//   - Non-standard output, reason="scriptpubkey"        → PASSES (post-Genesis exception)
//   - Non-standard output, reason!="scriptpubkey"       → FAILS  (dust, bare-multisig, etc.)
//   - acceptNonStandardOutput=false, non-standard input → FAILS  (stage 2 triggered)
ScriptError checkStandardness(
    const task::CCancellationToken& token,
    const ConfigScriptPolicy& policySettings,
    const CTransaction& tx,
    const bsv::CMutableTransactionExtended& eTX,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight
)
{
    // Replicate TxnValidation standardness logic from bitcoin-sv validation.cpp.
    // acceptNonStandardOutput is era-dependent:
    //   pre-Genesis  → !requireStandard
    //   post-Genesis → the explicit acceptNonStandardOutput policy setting (default true)
    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight);
    const bool acceptNonStandardOutput = policySettings.GetAcceptNonStandardOutput(era);

    std::string reason;
    if (!IsStandardTx(policySettings, tx, blockHeight, reason)) {
        // Mirror the bitcoin-sv rejection condition:
        //   always reject if acceptNonStandardOutput is false, OR
        //   in Genesis era, reject only when requireStandard is true AND
        //   the failure reason is not "scriptpubkey" (unrecognised output type),
        //   because post-Genesis nodes deliberately allow arbitrary output scripts.
        if (!acceptNonStandardOutput ||
            (IsProtocolActive(era, ProtocolName::Genesis) && policySettings.GetRequireStandard() && reason != "scriptpubkey")) {
            return SCRIPT_ERR_UNKNOWN_ERROR;
        }
    }

    // AreInputsStandard equivalent — only enforced when non-standard outputs are not accepted,
    // mirroring the bitcoin-sv TxnValidation path (validation.cpp:1208).
    if (!acceptNonStandardOutput && !tx.IsCoinBase()) {
        constexpr bool consensus = false;
        constexpr uint32_t flags = SCRIPT_VERIFY_NONE;
        const auto params = make_eval_script_params(policySettings, flags, consensus);

        for (size_t i = 0; i < tx.vin.size(); ++i) {
            const CScript& scriptSig = tx.vin[i].scriptSig;
            const CScript& prevScript = eTX.vutxo[i].scriptPubKey;
            const ProtocolEra utxoEra = GetProtocolEra(policySettings, utxoHeights[i]);

            auto result = IsInputStandard(token, params, scriptSig, prevScript, utxoEra, flags);
            if (!result.has_value() || !result.value()) {
                return SCRIPT_ERR_UNKNOWN_ERROR;
            }
        }
    }

    return SCRIPT_ERR_OK;
}

ScriptError bsv::CScriptEngine::VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags) const
{
    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty()) {
        throw std::runtime_error("error serializing extended tx");
    }

    if (eTX.vutxo.size() != utxoHeights.size()) {
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    }
    if (eTX.vutxo.size() != eTX.mtx.vin.size()) {
        throw std::runtime_error("inconsistent inputs size");
    }

    if (eTX.vutxo.empty() || eTX.mtx.vin.empty() || utxoHeights.empty()) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    if (!customFlags.empty() && customFlags.size() != eTX.vutxo.size()) {
        throw std::runtime_error("inconsistent utxo heights and number of custom flags");
    }

    const CTransaction ctx(eTX.mtx); // costly conversion due to hash calculation

    // Perform standardness checks when consensus=false
    const bool requireStandard = policySettings.GetRequireStandard();
    if (!consensus && requireStandard) {
        ScriptError standardnessError = checkStandardness(source->GetToken(), policySettings, ctx, eTX, utxoHeights, blockHeight);
        if (standardnessError != SCRIPT_ERR_OK) {
            return standardnessError;
        }
    }

    const bool useCustomFlags = !customFlags.empty();
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const int32_t utxoHeight{ utxoHeights[index] };
        const uint32_t flags = !useCustomFlags ? CalculateFlags(utxoHeight, blockHeight, consensus) : customFlags[index];

        ScriptError verifyError { SCRIPT_ERR_UNKNOWN_ERROR };
        if (!ctx.vin.empty() && !ctx.vout.empty())
        {
            const Amount amt{ amount };
            bsv::CachingScriptChecker sig_checker(&ctx, index, amt);
            verifyError = verifyImpl(uscript,
                lscript,
                consensus,
                flags,
                sig_checker);
        }
        else
        {
            BaseSignatureChecker sig_checker;
            verifyError = verifyImpl(uscript,
                lscript,
                consensus,
                flags,
                sig_checker);
        }

        // Return first error encountered
        if (verifyError != SCRIPT_ERR_OK) {
            return verifyError;
        }
    }

    return SCRIPT_ERR_OK;
}

std::vector<ScriptError> bsv::CScriptEngine::VerifyScriptBatch(const VerifyBatch& batch) const
{
    std::vector<ScriptError> results;
    results.reserve(batch.size());

    for (const auto& elem : batch) {
        ScriptError result = VerifyScript(
            elem.extendedTX,
            elem.utxoHeights,
            elem.blockHeight,
            elem.consensus,
            elem.customFlags
        );
        results.push_back(result);
    }

    return results;
}

ScriptError bsv::CScriptEngine::verifyImpl(
    const CScript& unlocking_script,
    const CScript& locking_script,
    const bool consensus,
    const unsigned int flags,
    BaseSignatureChecker& sig_checker
) const
{
    // Call of the bsv VerifyScript
    const verify_script_params veriParams{make_verify_script_params(policySettings, flags, consensus)};
    const auto ret = ::VerifyScript(veriParams,
        source->GetToken(),
        unlocking_script,
        locking_script,
        flags,
        sig_checker);
    
    if (!ret.has_value())
        return SCRIPT_ERR_UNKNOWN_ERROR;
    
    return ret.value();
}

int bsv::CPP_SCRIPT_ERR_ERROR_COUNT(){
    return SCRIPT_ERR_ERROR_COUNT;
}