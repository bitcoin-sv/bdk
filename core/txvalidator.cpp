#include <base58.h>
#include <core_io.h>
#include <protocol_era.h>
#include <policy/policy.h>
#include <verify_script_flags.h>
#include <script/script_flags.h>
#include <script/standard.h>

#include <chainparams_bdk.hpp>
#include <extendedTx.hpp>
#include <txvalidator.hpp>

bsv::CTxValidator::CTxValidator(const std::string chainName)
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

bool bsv::CTxValidator::SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* err)
{
    return policySettings.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, err);
}

bool bsv::CTxValidator::SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err)
{
    return policySettings.SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, err);
}

bool bsv::CTxValidator::SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err)
{
    return policySettings.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, err);
}

bool bsv::CTxValidator::SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err)
{
    return policySettings.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, err);
}

bool bsv::CTxValidator::SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err)
{
    return policySettings.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, err);
}

bool bsv::CTxValidator::SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err)
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

bool bsv::CTxValidator::SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err)
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

uint64_t bsv::CTxValidator::GetMaxOpsPerScript(bool isGenesisEnabled, bool consensus) const
{
    return policySettings.GetMaxOpsPerScript(isGenesisEnabled, consensus);
}

uint64_t bsv::CTxValidator::GetMaxScriptNumLength(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
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

uint64_t bsv::CTxValidator::GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CTxValidator::GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CTxValidator::GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t bsv::CTxValidator::GetGenesisActivationHeight() const
{
    return policySettings.GetGenesisActivationHeight();
}

int32_t bsv::CTxValidator::GetChronicleActivationHeight() const
{
    return policySettings.GetChronicleActivationHeight();
}

uint64_t bsv::CTxValidator::GetGenesisGracefulPeriod() const
{
    return policySettings.GetGenesisGracefulPeriod();
}

uint64_t bsv::CTxValidator::GetChronicleGracefulPeriod() const
{
    return policySettings.GetChronicleGracefulPeriod();
}

bool bsv::CTxValidator::SetGenesisGracefulPeriod(int64_t genesisGracefulPeriodIn, std::string* err)
{
    return policySettings.SetGenesisGracefulPeriod(genesisGracefulPeriodIn, err);
}

bool bsv::CTxValidator::SetChronicleGracefulPeriod(int64_t chronicleGracefulPeriodIn, std::string* err)
{
    return policySettings.SetChronicleGracefulPeriod(chronicleGracefulPeriodIn, err);
}

bool bsv::CTxValidator::SetMaxTxSizePolicy(int64_t value, std::string* err)
{
    return policySettings.SetMaxTxSizePolicy(value, err);
}

void bsv::CTxValidator::SetDataCarrierSize(uint64_t dataCarrierSize)
{
    policySettings.SetDataCarrierSize(dataCarrierSize);
}

void bsv::CTxValidator::SetDataCarrier(bool dataCarrier)
{
    policySettings.SetDataCarrier(dataCarrier);
}

void bsv::CTxValidator::SetAcceptNonStandardOutput(bool accept)
{
    policySettings.SetAcceptNonStandardOutput(accept);
}

void bsv::CTxValidator::SetRequireStandard(bool require)
{
    policySettings.SetRequireStandard(require);
}

void bsv::CTxValidator::SetPermitBareMultisig(bool permit)
{
    policySettings.SetPermitBareMultisig(permit);
}

void bsv::CTxValidator::ResetDefault()
{
    policySettings.ResetDefault();
    consolidationMinFactor = 20;
    consolidationMaxInputScriptSize = 150;
    consolidationMinConf = 6;
    consolidationAcceptNonStd = false;
}

void bsv::CTxValidator::SetMinConsolidationFactor(uint64_t value) { consolidationMinFactor = value; }
void bsv::CTxValidator::SetMaxConsolidationInputScriptSize(uint64_t value) { consolidationMaxInputScriptSize = value; }
void bsv::CTxValidator::SetMinConfConsolidationInput(uint64_t value) { consolidationMinConf = value; }
void bsv::CTxValidator::SetAcceptNonStdConsolidationInput(bool value) { consolidationAcceptNonStd = value; }

uint64_t bsv::CTxValidator::GetMinConsolidationFactor() const { return consolidationMinFactor; }
uint64_t bsv::CTxValidator::GetMaxConsolidationInputScriptSize() const { return consolidationMaxInputScriptSize; }
uint64_t bsv::CTxValidator::GetMinConfConsolidationInput() const { return consolidationMinConf; }
bool bsv::CTxValidator::GetAcceptNonStdConsolidationInput() const { return consolidationAcceptNonStd; }

uint64_t bsv::CTxValidator::GetMaxTxSize(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
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

uint64_t bsv::CTxValidator::GetDataCarrierSize() const
{
    return policySettings.GetDataCarrierSize();
}

bool bsv::CTxValidator::GetDataCarrier() const
{
    return policySettings.GetDataCarrier();
}

bool bsv::CTxValidator::GetAcceptNonStandardOutput(bool isGenesisEnabled, bool isChronicleEnabled) const
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

bool bsv::CTxValidator::GetRequireStandard() const
{
    return policySettings.GetRequireStandard();
}

bool bsv::CTxValidator::GetPermitBareMultisig() const
{
    return policySettings.GetPermitBareMultisig();
}

uint32_t bsv::CTxValidator::CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const
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
        const bool requireStandard = policySettings.GetRequireStandard();
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
uint64_t bsv::CTxValidator::GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight) const {
    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty()) {
        throw std::runtime_error("error serializing extended tx");
    }

    const CTransaction ctx(eTX.mtx);
    return implGetSigOpCount(ctx, eTX.vutxo, utxoHeights, blockHeight);
}

uint64_t bsv::CTxValidator::implGetSigOpCount(
    const CTransaction& ctx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight) const
{
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
    if (prevUTXO.size() != ctx.vin.size()) {
        throw std::runtime_error("inconsistent inputs size");
    }
    if (prevUTXO.size() != utxoHeights.size()) {
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    }

    uint64_t nSigOpsP2SH{0};
    for (size_t index = 0; index < prevUTXO.size(); ++index) {
        const ProtocolEra utxoEra = GetProtocolEra(policySettings, utxoHeights[index]);
        if (IsProtocolActive(utxoEra, ProtocolName::Genesis)) {
            continue;
        }

        const CTxOut& prevout = prevUTXO[index];
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


TxError bsv::CTxValidator::VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing extended tx");

        const CTransaction ctx(eTX.mtx);
        return implVerifyScript(ctx, eTX.vutxo, utxoHeights, blockHeight, consensus, customFlags);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

std::vector<TxError> bsv::CTxValidator::VerifyScriptBatch(const VerifyBatch& batch) const
{
    std::vector<TxError> results;
    results.reserve(batch.size());

    for (const auto& elem : batch) {
        results.push_back(VerifyScript(
            elem.extendedTX,
            elem.utxoHeights,
            elem.blockHeight,
            elem.consensus,
            elem.customFlags
        ));
    }

    return results;
}

TxError bsv::CTxValidator::CheckStandardness(std::span<const uint8_t> extendedTX,
                                              std::span<const int32_t> utxoHeights,
                                              int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        const CTransaction ctx(eTX.mtx);
        return implCheckStandardness(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::implVerifyScript(
    const CTransaction& ctx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight,
    bool consensus,
    std::span<const uint32_t> customFlags
) const
{
    if (prevUTXO.size() != utxoHeights.size())
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    if (prevUTXO.size() != ctx.vin.size())
        throw std::runtime_error("inconsistent inputs size");
    if (prevUTXO.empty() || ctx.vin.empty() || utxoHeights.empty())
        throw std::runtime_error("tx has no inputs to verify");
    if (!customFlags.empty() && customFlags.size() != prevUTXO.size())
        throw std::runtime_error("inconsistent custom flags size");

    const bool useCustomFlags = !customFlags.empty();
    for (size_t index = 0; index < prevUTXO.size(); ++index) {
        const uint64_t amount = prevUTXO[index].nValue.GetSatoshis();
        const CScript& lscript = prevUTXO[index].scriptPubKey;
        const CScript& uscript = ctx.vin[index].scriptSig;

        const int32_t utxoHeight{ utxoHeights[index] };
        const uint32_t flags = !useCustomFlags ? CalculateFlags(utxoHeight, blockHeight, consensus) : customFlags[index];

        TxError verifyResult = bsv::TxErrorOk();
        if (!ctx.vin.empty() && !ctx.vout.empty()) {
            const Amount amt{ amount };
            TransactionSignatureChecker sig_checker(&ctx, index, amt);
            verifyResult = bsvVerifyScript(uscript, lscript, consensus, flags, sig_checker);
        } else {
            BaseSignatureChecker sig_checker;
            verifyResult = bsvVerifyScript(uscript, lscript, consensus, flags, sig_checker);
        }

        if (!bsv::TxErrorIsOk(verifyResult))
            return verifyResult;
    }
    return bsv::TxErrorOk();
}

TxError bsv::CTxValidator::bsvVerifyScript(
    const CScript& unlocking_script,
    const CScript& locking_script,
    const bool consensus,
    const unsigned int flags,
    BaseSignatureChecker& sig_checker
) const
{
    const verify_script_params veriParams{make_verify_script_params(policySettings, flags, consensus)};
    const auto ret = ::VerifyScript(veriParams,
        source->GetToken(),
        unlocking_script,
        locking_script,
        flags,
        sig_checker);

    if (!ret.has_value())
        return bsv::TxErrorException();  // cancellation token fired

    if (ret.value() == SCRIPT_ERR_OK)
        return bsv::TxErrorOk();

    return bsv::TxErrorScript(ret.value());
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
TxError bsv::CTxValidator::implCheckStandardness(
    const CTransaction& tx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight
) const
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
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotStandard));
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
            const CScript& prevScript = prevUTXO[i].scriptPubKey;
            const ProtocolEra utxoEra = GetProtocolEra(policySettings, utxoHeights[i]);

            auto result = IsInputStandard(source->GetToken(), params, scriptSig, prevScript, utxoEra, flags);
            if (!result.has_value() || !result.value()){
                return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotStandard));
            }
        }
    }

    return bsv::TxErrorOk();
}

int bsv::CPP_SCRIPT_ERR_ERROR_COUNT(){
    return SCRIPT_ERR_ERROR_COUNT;
}

TxError bsv::CTxValidator::CheckTransaction(std::span<const uint8_t> extendedTX,
                                             std::span<const int32_t> utxoHeights,
                                             int32_t blockHeight,
                                             bool consensus) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing extended tx");

        const CTransaction ctx(eTX.mtx);

        if (auto r = implCheckPrevOutputs(ctx);                                               !bsv::TxErrorIsOk(r)) return r;
        if (auto r = implCheckOutputs(ctx, blockHeight);                                       !bsv::TxErrorIsOk(r)) return r;
        if (auto r = implCheckConsensusSigops(ctx, blockHeight);                               !bsv::TxErrorIsOk(r)) return r;
        if (!consensus) {
            if (policySettings.GetRequireStandard()) {
                if (auto r = implCheckStandardness(ctx, eTX.vutxo, utxoHeights, blockHeight); !bsv::TxErrorIsOk(r)) return r;
            }
            if (auto r = implCheckSigOpsPolicy(ctx, eTX.vutxo, utxoHeights, blockHeight);     !bsv::TxErrorIsOk(r)) return r;
            if (auto r = implIsFreeConsolidation(ctx, eTX.vutxo, utxoHeights, blockHeight);   !bsv::TxErrorIsOk(r)) return r;
        }
        return implVerifyScript(ctx, eTX.vutxo, utxoHeights, blockHeight, consensus);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckPrevOutputs(std::span<const uint8_t> extendedTX) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckPrevOutputs(ctx);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckOutputs(std::span<const uint8_t> extendedTX, int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckOutputs(ctx, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckConsensusSigops(std::span<const uint8_t> extendedTX, int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckConsensusSigops(ctx, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckSigOpsPolicy(std::span<const uint8_t> extendedTX,
                                              std::span<const int32_t> utxoHeights,
                                              int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckSigOpsPolicy(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::IsFreeConsolidation(std::span<const uint8_t> extendedTX,
                                                std::span<const int32_t> utxoHeights,
                                                int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implIsFreeConsolidation(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

// TODO: implement — reject inputs where prevout txid is all-zeros and index is 0xFFFFFFFF.
TxError bsv::CTxValidator::implCheckPrevOutputs(const CTransaction& tx) const
{
    return bsv::TxErrorOk();
}

// TODO: implement — reject P2SH outputs after Genesis activation height.
TxError bsv::CTxValidator::implCheckOutputs(const CTransaction& tx, int32_t blockHeight) const
{
    return bsv::TxErrorOk();
}

// TODO: implement — enforce pre-Genesis 20,000 sigop limit without P2SH.
TxError bsv::CTxValidator::implCheckConsensusSigops(const CTransaction& tx, int32_t blockHeight) const
{
    return bsv::TxErrorOk();
}

// TODO: implement — enforce configurable policy sigop limit including P2SH redeem scripts.
TxError bsv::CTxValidator::implCheckSigOpsPolicy(const CTransaction& tx,
                                                   const std::vector<CTxOut>& prevUTXO,
                                                   std::span<const int32_t> utxoHeights,
                                                   int32_t blockHeight) const
{
    return bsv::TxErrorOk();
}

// Replicates IsFreeConsolidationTxn from bitcoin-sv policy/policy.cpp.
// Returns OK if the transaction qualifies as a fee-exempt consolidation (or dust donation).
// Returns NotFreeConsolidation otherwise.
TxError bsv::CTxValidator::implIsFreeConsolidation(const CTransaction& tx,
                                                     const std::vector<CTxOut>& prevUTXO,
                                                     std::span<const int32_t> utxoHeights,
                                                     int32_t blockHeight) const
{
    constexpr auto notFree = []() {
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotFreeConsolidation));
    };

    if (consolidationMinFactor == 0)
        return notFree();

    if (tx.IsCoinBase())
        return notFree();

    // Inline equivalent of IsDustReturnTxn (policy/policy.cpp is not linked in bdk_core)
    const bool isDonation = tx.vout.size() == 1
        && tx.vout[0].nValue.GetSatoshis() == 0
        && IsDustReturnScript(tx.vout[0].scriptPubKey);
    const uint64_t factor = isDonation ? tx.vin.size() : consolidationMinFactor;
    const int32_t minConf = isDonation ? int32_t(0) : static_cast<int32_t>(consolidationMinConf);

    if (tx.vin.size() < factor * tx.vout.size())
        return notFree();

    if (prevUTXO.size() != tx.vin.size() || utxoHeights.size() != tx.vin.size())
        return notFree();

    uint64_t sumInputScriptPubKeySize = 0;
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        const int32_t coinHeight = utxoHeights[i];

        if (minConf > 0 && coinHeight == MEMPOOL_HEIGHT)
            return notFree();

        const int32_t seenConf = blockHeight + 1 - coinHeight;
        if (minConf > 0 && coinHeight && seenConf < minConf)
            return notFree();

        if (tx.vin[i].scriptSig.size() > consolidationMaxInputScriptSize)
            return notFree();

        txnouttype dummyType;
        if (!consolidationAcceptNonStd && !IsStandardOutput(policySettings, prevUTXO[i].scriptPubKey, coinHeight, dummyType))
            return notFree();

        sumInputScriptPubKeySize += prevUTXO[i].scriptPubKey.size();
    }

    uint64_t sumOutputScriptPubKeySize = 0;
    for (const CTxOut& o : tx.vout)
        sumOutputScriptPubKeySize += o.scriptPubKey.size();

    if (sumInputScriptPubKeySize < factor * sumOutputScriptPubKeySize)
        return notFree();

    return bsv::TxErrorOk();
}