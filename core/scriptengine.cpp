#include <base58.h>
#include <protocol_era.h>
#include <core_io.h>
#include <verify_script_flags.h>
#include <script/script_flags.h>

#include <chainparams_bdk.hpp>
#include <interpreter_bdk.hpp>
#include <extendedTx.hpp>
#include <scriptengine.hpp>

bsv::CScriptEngine::CScriptEngine(const std::string chainName)
    : chainParams{ std::move(bsv::CreateCustomChainParams(chainName)) }
    , source{ task::CCancellationSource::Make() }
{
    std::string errStr;
    bool ok {true};

    int32_t genesisHeight;
    int32_t chronicleHeight;
    ok = ok && this->SetGenesisActivationHeight(chainParams->GetConsensus().genesisHeight, &errStr);
    ok = ok && this->SetChronicleActivationHeight(chainParams->GetConsensus().chronicleHeight, &errStr);
    if (!(ok || errStr.empty())){
        throw std::runtime_error("error setting genesis and chronicle heights " + errStr);
    }
}


const GlobalConfig& bsv::CScriptEngine::GetGlobalConfig() {
    return bsvConfig;
}

bool bsv::CScriptEngine::SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* err)
{
    return bsvConfig.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, err);
}

bool bsv::CScriptEngine::SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err)
{
    return bsvConfig.SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, err);
}

bool bsv::CScriptEngine::SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err)
{
    return bsvConfig.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, err);
}

bool bsv::CScriptEngine::SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err)
{
    return bsvConfig.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, err);
}

bool bsv::CScriptEngine::SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err)
{
    return bsvConfig.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, err);
}

bool bsv::CScriptEngine::SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && bsvConfig.SetGenesisActivationHeight(genesisActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto chronicleHeight = bsvConfig.GetChronicleActivationHeight();
    if ( chronicleHeight < bsvConfig.GetGenesisActivationHeight() ) {
        ok = ok && bsvConfig.SetChronicleActivationHeight(genesisActivationHeightIn, err);
    }

    return ok;
}

bool bsv::CScriptEngine::SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && bsvConfig.SetChronicleActivationHeight(chronicleActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto genesisHeight = bsvConfig.GetGenesisActivationHeight();
    if ( bsvConfig.GetChronicleActivationHeight() < genesisHeight ) {
        ok = ok && bsvConfig.SetGenesisActivationHeight(chronicleActivationHeightIn, err);
    }

    return ok;
}

uint64_t bsv::CScriptEngine::GetMaxOpsPerScript(bool isGenesisEnabled, bool consensus) const
{
    return bsvConfig.GetMaxOpsPerScript(isGenesisEnabled, consensus);
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

    return bsvConfig.GetMaxScriptNumLength(era, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const
{
    return bsvConfig.GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const
{
    return bsvConfig.GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CScriptEngine::GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const
{
    return bsvConfig.GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t bsv::CScriptEngine::GetGenesisActivationHeight() const
{
    return bsvConfig.GetGenesisActivationHeight();
}

int32_t bsv::CScriptEngine::GetChronicleActivationHeight() const
{
    return bsvConfig.GetChronicleActivationHeight();
}

uint32_t bsv::CScriptEngine::CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const
{
    ProtocolEra era;
    uint32_t protocolFlags;
    if (consensus) {
        // For tx coming from a block.
        // blockHeight is the block the tx belong to
        // In that case, the chainTip is blockHeight-1
        era = GetProtocolEra(bsvConfig, blockHeight );
        const Consensus::Params& consensusparams = chainParams->GetConsensus();
        protocolFlags = GetBlockScriptFlags(consensusparams, blockHeight - 1 , era);
    } else {
        // For tx coming from a peer
        // blockHeight is the chain tip (highest current block)
        era = GetProtocolEra(bsvConfig, blockHeight + 1);
        const bool requireStandard = chainParams->RequireStandard();
        protocolFlags = GetScriptVerifyFlags(era, requireStandard);
    }

    ProtocolEra utxoEra{ GetProtocolEra(bsvConfig, utxoHeight) };
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
    const auto era = GetProtocolEra(bsvConfig, blockHeight + 1);


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
        const ProtocolEra utxoEra = GetProtocolEra(bsvConfig, utxoHeights[index]);
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

bitcoinconsensus_error bsv::CScriptEngine::CheckConsensus(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, std::span<const uint32_t> customFlags) const {
    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream in_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    const auto in_size = in_stream.size();
    bsv::CMutableTransactionExtended eTX;

    try {
        in_stream >> eTX;
    }
    catch (...) {
        return bitcoinconsensus_ERR_TX_DESERIALIZE;
    }



    if (!in_stream.empty()) {
        return bitcoinconsensus_ERR_TX_DESERIALIZE;
    }

    if (eTX.vutxo.size() != utxoHeights.size()) {
        return bitcoinconsensus_ERR_TX_INDEX;
    }
    if (eTX.vutxo.size() != eTX.mtx.vin.size()) {
        throw bitcoinconsensus_ERR_TX_INDEX;
    }

    if (eTX.vutxo.empty() || eTX.mtx.vin.empty() || utxoHeights.empty()) {
        return bitcoinconsensus_ERR_TX_DESERIALIZE;
    }

    if (!customFlags.empty() && customFlags.size() != eTX.vutxo.size()) {
        return bitcoinconsensus_ERR_TX_INDEX;
    }

    const CTransaction ctx(eTX.mtx); // costly conversion due to hash calculation
    std::atomic<malleability::status> ms{};
    const bool useCustomFlags = !customFlags.empty();
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const int32_t utxoHeight{ utxoHeights[index] };
        const bool consensus = true;

        uint32_t flags;
        if (!useCustomFlags) {
            flags == CalculateFlags(utxoHeight, blockHeight, consensus);
            flags &= SCRIPT_VERIFY_NULLDUMMY; // SCRIPT_VERIFY_NULLDUMMY was missing in GetBlockScriptFlags
        }
        else {
            flags = customFlags[index];
        }

        if (!(flags & ~(bitcoinconsensus_SCRIPT_FLAGS_VERIFY_ALL)) == 0) {
            // verify_flags
            return bitcoinconsensus_ERR_INVALID_FLAGS;
        }
    }

    // Re serialize transaction and make sure the recovered stream match the input stream size
    CDataStream out_stream(SER_NETWORK, PROTOCOL_VERSION);
    out_stream << eTX;
    if (in_size != out_stream.size()) {
        return bitcoinconsensus_ERR_TX_SIZE_MISMATCH;
    }

    return bitcoinconsensus_ERR_OK;
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
    std::atomic<malleability::status> ms{};
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
            TransactionSignatureChecker sig_checker(&ctx, index, amt);
            verifyError = verifyImpl(uscript,
                lscript,
                consensus,
                flags,
                sig_checker,
                ms);
        }
        else
        {
            BaseSignatureChecker sig_checker;
            verifyError = verifyImpl(uscript,
                lscript,
                consensus,
                flags,
                sig_checker,
                ms);
        }

        // Return first error encountered
        if (verifyError != SCRIPT_ERR_OK) {
            return verifyError;
        }
    }

    return SCRIPT_ERR_OK;
}

ScriptError bsv::CScriptEngine::verifyImpl(
    const CScript& unlocking_script,
    const CScript& locking_script,
    const bool consensus,
    const unsigned int flags,
    BaseSignatureChecker& sig_checker,
    std::atomic<malleability::status>& malleability
) const
{
    // Call of the bsv VerifyScript
    const verify_script_params veriParams{make_verify_script_params(bsvConfig, flags, consensus)};
    const auto ret = ::VerifyScript(veriParams,
        source->GetToken(),
        unlocking_script,
        locking_script,
        flags,
        sig_checker,
        malleability);
    return helperOptional2ScriptError(ret);
}

ScriptError bsv::CScriptEngine::helperOptional2ScriptError(const std::optional<std::pair<bool, ScriptError>>& ret)  const {
    // If the returned result is empty, we don't know what kind of error is that
    if (!ret.has_value()) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    // If the returned result is true and error code is not OK, then we don't know what happened
    if (ret->first && ret->second != SCRIPT_ERR_OK) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    return ret->second;
}

int bsv::CPP_SCRIPT_ERR_ERROR_COUNT(){
    return SCRIPT_ERR_ERROR_COUNT;
}