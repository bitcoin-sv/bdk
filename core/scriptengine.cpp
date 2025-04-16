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
    return bsvConfig.SetGenesisActivationHeight(genesisActivationHeightIn, err);
}

bool bsv::CScriptEngine::SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err)
{
    return bsvConfig.SetChronicleActivationHeight(chronicleActivationHeightIn, err);
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
        era = GetProtocolEra(bsvConfig, blockHeight);
        const bool requireStandard = chainParams->RequireStandard();
        protocolFlags = GetScriptVerifyFlags(era, requireStandard);
    } else {
        era = GetProtocolEra(bsvConfig, blockHeight + 1);
        const Consensus::Params& consensusparams = chainParams->GetConsensus();
        protocolFlags = GetBlockScriptFlags(consensusparams, blockHeight, era);
    }

    ProtocolEra utxoEra{ GetProtocolEra(bsvConfig, utxoHeight) };
    const uint32_t utxoFlags { InputScriptVerifyFlags(era, utxoEra) };

    return (protocolFlags | utxoFlags);
}

ScriptError bsv::CScriptEngine::VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus) const
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

    const CTransaction ctx(eTX.mtx); // costly conversion due to hash calculation
    std::atomic<malleability::status> ms{};
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const int32_t utxoHeight{ utxoHeights[index] };
        const uint32_t flags = CalculateFlags(utxoHeight, blockHeight, consensus);

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
    const auto ret = ::VerifyScript(bsvConfig,
        consensus,
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