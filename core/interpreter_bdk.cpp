#include <interpreter_bdk.hpp>
#include <extendedTx.hpp>
#include <chainparams_bdk.hpp>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <protocol_era.h>
#include <script/script.h>
#include <core_io.h>
#include <script/interpreter.h>
#include <script/script_flags.h>
#include <script/malleability_status.h>

#include <stdexcept>
#include <iostream>
#include <chrono>

using namespace std;

// These definitions are copied from $BSV_ROOT/src/chainparams.cpp
// We use them to know what chain we are on, by reading the consensus
// variable consensus.genesisHeight
#define GENESIS_ACTIVATION_MAIN                 620538
#define GENESIS_ACTIVATION_STN                  100
#define GENESIS_ACTIVATION_TESTNET              1344302
#define GENESIS_ACTIVATION_REGTEST              10000

std::string bsv::SetGlobalScriptConfig(
    std::string chainNetwork,
    int64_t maxOpsPerScriptPolicyIn,
    int64_t maxScriptNumLengthPolicyIn,
    int64_t maxScriptSizePolicyIn,
    int64_t maxPubKeysPerMultiSigIn,
    int64_t maxStackMemoryUsageConsensusIn,
    int64_t maxStackMemoryUsagePolicyIn,
    int32_t customGenesisHeight
)
{
    std::string err;
    try {

        auto& gConfig = GlobalConfig::GetModifiableGlobalConfig();

        // Set the Chain Params and the genesis/chronicle height in config.
        // We have to set these heights because they are used inside the config
        // not inside the ChainParams
        bsv::select_params(chainNetwork); // throw exception if wrong network string
        const CChainParams& chainparams = gConfig.GetChainParams(); // ChainParams after setting the chain network
        const int32_t gh = (customGenesisHeight > 0) ? customGenesisHeight : chainparams.GetConsensus().genesisHeight;
        if (!gConfig.SetGenesisActivationHeight(gh, &err)) {
            throw std::runtime_error("unable to set genesis activation height : " + err);
        }
        if (!gConfig.SetChronicleActivationHeight(chainparams.GetConsensus().chronicleHeight, &err)) {
            throw std::runtime_error("unable to set chronicle activation height : " + err);
        }
        if (gh > chainparams.GetConsensus().chronicleHeight) {
            throw std::runtime_error("genesis activation height was set higher than chronicle height");
        }

        bool ok=true;
        if (maxOpsPerScriptPolicyIn > 0)
            ok = ok && gConfig.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, &err);
        if (maxScriptNumLengthPolicyIn > 0)
            ok = ok && gConfig.SetMaxScriptNumLengthPolicy(maxScriptNumLengthPolicyIn, &err);
        if (maxScriptSizePolicyIn > 0)
            ok = ok && gConfig.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, &err);
        if (maxPubKeysPerMultiSigIn > 0)
            ok = ok &&  gConfig.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, &err);
        if (maxStackMemoryUsageConsensusIn > 0 || maxStackMemoryUsagePolicyIn > 0)
            ok = ok && gConfig.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, &err);

        if (err.empty() && !ok ) {
            err = "Unknown error while setting global config for script";
        }

        if (err.empty()) {
            return err;
        }

    } catch (const std::exception& e) {
        std::stringstream ss;
        ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ << " "<< e.what()<<std::endl;
        err = ss.str();
    }

    return err;
}

int32_t bsv::GetGenesisActivationHeight() {
    auto& gConfig = GlobalConfig::GetConfig();
    return gConfig.GetGenesisActivationHeight();
}

// Check if the P2SH script is activated on different chains. We provide our own approach
// to replace the BSV code
//
//     https://github.com/bitcoin-sv/bitcoin-sv/blob/master/src/validation.cpp#L2882
//
// As we don't have the block time in our calculations, we use the precise block heigh
// to detect the P2SH_ACTIVATION_TIME
//
//     https://github.com/bitcoin-sv/teranode/issues/2466
bool is_p2sh_activated(const Consensus::Params& params, int32_t blockHeight) {
    // For mainnet block activating p2sh is 173805
    if (params.hashGenesisBlock == uint256S("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f") && blockHeight > 173804) {
        return true;
    }

    // For testnet block activating p2sh is 519
    if (params.hashGenesisBlock == uint256S("000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943") && blockHeight > 518) {
        return true;
    }

    // For any other network, it's probably created long time after P2SH_ACTIVATION_TIME, so p2sh is always activated
    return true;
}

// clone and modified of GetScriptVerifyFlags
//
// get_script_verify_flags is a clone of GetScriptVerifyFlags in chronicle version
// In chronical version, GetScriptVerifyFlags is defined in $BSV/src/validation.cpp, if we include this file
// into our compilation, what'll include a lot of other files that make the whole dependencies explode.
// This is why we'd rather clone this function here and use it.
//
// The original version used ProtocolEra as input, we used blockHeight as input, to make it similar to the
// call of get_block_script_flags
uint32_t get_script_verify_flags(const Config& config, int32_t blockHeight)
{
    const ProtocolEra era{ GetProtocolEra(config, blockHeight) };

    // Get verification flags for overall script - individual UTXOs may need
    // to add/remove flags (done by CheckInputScripts).
    uint32_t scriptVerifyFlags{ StandardScriptVerifyFlags(era) };
    if (!config.GetChainParams().RequireStandard())
    {
        if (config.IsSetPromiscuousMempoolFlags())
        {
            scriptVerifyFlags = config.GetPromiscuousMempoolFlags();
        }
        scriptVerifyFlags |= SCRIPT_ENABLE_SIGHASH_FORKID;
    }

    return scriptVerifyFlags;
}

// clone and modified of GetBlockScriptFlags
//
// get_block_script_flags is a clone and slight modified version of GetBlockScriptFlags.
// Same reason as GetScriptVerifyFlags, we don't want to include $BSV/src/validation.cpp that will explode
// dependencies, we clone it here. We also modify it slightly as the native GetBlockScriptFlags use the struct
// BlockIndex as input, while we don't have that, we use blockHeight
//
// This slight modification skip the handling of grace period using GetMedianTimePast
// See
//    https://github.com/bitcoin-sv/bitcoin-sv/blob/master/src/validation.cpp#L2882
uint32_t get_block_script_flags(const Config& config, int32_t blockHeight) {
    const Consensus::Params& consensusparams = config.GetChainParams().GetConsensus();
    uint32_t flags = SCRIPT_VERIFY_NONE;

    // We check if the utxo is P2SH by the content of the locking script, rather
    // than by the block height of the utxo
    try {
        if (is_p2sh_activated(consensusparams, blockHeight)) {
            flags |= SCRIPT_VERIFY_P2SH;
        }
    }
    catch (const std::exception& e) {
    }

    // Start enforcing the DERSIG (BIP66) rule
    if ((blockHeight + 1) >= consensusparams.BIP66Height) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
    if ((blockHeight + 1) >= consensusparams.BIP65Height) {
        flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    }

    // Start enforcing BIP112 (CSV).
    if ((blockHeight + 1) >= consensusparams.CSVHeight) {
        flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    }

    // If the UAHF is enabled, we start accepting replay protected txns
    if (blockHeight >= consensusparams.uahfHeight) { // (IsUAHFenabled(config, blockHeight))
        flags |= SCRIPT_VERIFY_STRICTENC;
        flags |= SCRIPT_ENABLE_SIGHASH_FORKID;
    }

    // If the DAA HF is enabled, we start rejecting transaction that use a high
    // s in their signature. We also make sure that signature that are supposed
    // to fail (for instance in multisig or other forms of smart contracts) are
    // null.
    if (blockHeight >= config.GetChainParams().GetConsensus().daaHeight) { // (IsDAAEnabled(config, blockHeight))
        flags |= SCRIPT_VERIFY_LOW_S;
        flags |= SCRIPT_VERIFY_NULLFAIL;
    }

    if (IsProtocolActive(GetProtocolEra(config, blockHeight + 1), ProtocolName::Genesis)) {
        flags |= SCRIPT_GENESIS;
        flags |= SCRIPT_VERIFY_SIGPUSHONLY;
    }

    if (IsProtocolActive(GetProtocolEra(config, blockHeight + 1), ProtocolName::Chronicle)) {
        flags |= SCRIPT_CHRONICLE;
    }

    return flags;
}

// Use of
//   GetBlockScriptFlags     // Tx coming from a block
//   InputScriptVerifyFlags  // Tx coming from a  peer
uint32_t bsv::script_verification_flags(int32_t utxoHeight, int32_t blockHeight, bool consensus) {

    const Config& config = GlobalConfig::GetConfig();
    const ProtocolEra activeEra{ GetProtocolEra(config, blockHeight) };
    const ProtocolEra utxoEra{ GetProtocolEra(config, utxoHeight) };

    // If consensus = true , it is a tx coming from a block, we use GetBlockScriptFlags
    // If consensus = false, it is a tx coming from a peer , we use GetScriptVerifyFlags
    const uint32_t scriptVerifyFlags = consensus ? get_block_script_flags(config, blockHeight) : get_script_verify_flags(config, blockHeight) ;

    const uint32_t perInputScriptFlags{ InputScriptVerifyFlags(activeEra, utxoEra) };
    return scriptVerifyFlags | perInputScriptFlags;
}

// This method of flags calculation was replicated from $BSV/src/validation.cpp::GetBlockScriptFlags in Chronicle release 1.2.0
// The version implemented here has been slightly modified to adapt to the context of independant library
uint32_t bsv::script_verification_flags_v2(const std::span<const uint8_t> locking_script, int32_t blockHeight) {
    const Config& config = GlobalConfig::GetConfig();

    const uint32_t blockVerifyFlabgs = get_block_script_flags(config, blockHeight);

    // Trying to get the utxoEra using a combination of methods in $BSV/src/bitcoin-tx.cpp
    // To get the InputScriptVerifyFlags from the utxo
    const ProtocolEra activeEra{ GetProtocolEra(config, blockHeight) };
    const ProtocolEra utxoEra{ IsP2SH(locking_script) ? ProtocolEra::PreGenesis : activeEra };
    const uint32_t perInputScriptFlags{ InputScriptVerifyFlags(activeEra, utxoEra) };
    return blockVerifyFlabgs | perInputScriptFlags;
}


// This method of flags calculation was found in $BSV/src/bitcoin-tx.cpp in Chronicle release 1.2.0
// It is calculated based on the locking script and the boolean isPostChronical
// If the node parameter -genesis is set to true, then the argument isPostChronical is false
// Otherwise, isPostChronical is true
uint32_t bsv::script_verification_flags_v1(const std::span<const uint8_t> locking_script, const bool isPostChronical){
    // The core C++ code IsP2SH use index operator[22] without checking the size
    // which is dangerous (undefined behaviour). We check and throw exception here
    if (locking_script.size() < 23) {
        throw std::out_of_range("Invalid locking script size");
    }

    const ProtocolEra ActiveEra { isPostChronical ? ProtocolEra::PostChronicle : ProtocolEra::PostGenesis };
    const ProtocolEra utxoEra { IsP2SH(locking_script)? ProtocolEra::PreGenesis : ActiveEra };
    const uint32_t flags = (StandardScriptVerifyFlags(ActiveEra) | InputScriptVerifyFlags(ActiveEra, utxoEra));
    return flags;
}

ScriptError bsv::get_raw_eval_script(const std::optional<std::variant<ScriptError, malleability::status>>& ret){
    if (ret.has_value()) {
        const auto vErr{ret.value()};
        if(std::holds_alternative<ScriptError>(vErr)) {
            return std::get<ScriptError>(vErr);
        } else {
            const auto ms = std::get<malleability::status>(vErr);

            if (ms == malleability::non_malleable) {
                return SCRIPT_ERR_OK;
            }

            if (ms & malleability::unclean_stack) {
                return SCRIPT_ERR_CLEANSTACK;
            }

            if (ms & malleability::non_minimal_push) {
                return SCRIPT_ERR_MINIMALDATA;
            }

            if (ms & malleability::non_minimal_scriptnum) {
                return SCRIPT_ERR_SCRIPTNUM_MINENCODE;
            }

            if (ms & malleability::high_s) {
                return SCRIPT_ERR_SIG_HIGH_S;
            }

            if (ms & malleability::non_push_data) {
                return SCRIPT_ERR_SIG_PUSHONLY;
            }

            if (ms & malleability::disallowed) {
                return SCRIPT_ERR_UNKNOWN_ERROR;
            }
        }
    }

    return SCRIPT_ERR_UNKNOWN_ERROR;
}

    // This helper function transforming result of BSV::VerifyScript to a raw ScriptError
ScriptError bsv::get_raw_verify_script(const std::optional<std::pair<bool, ScriptError>>& ret){
    // If the returned result is empty, we don't know what kind of error is that
    if(!ret.has_value()) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    // If the returned result is true and error code is not OK, then we don't know what happened
    if (ret->first && ret->second != SCRIPT_ERR_OK) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    return ret->second;
}

namespace
{
    ScriptError execute_impl(const CScript& script,
                             const bool consensus,
                             const unsigned int flags,
                             BaseSignatureChecker& sig_checker)
    {
        auto source = task::CCancellationSource::Make();
        LimitedStack stack(UINT32_MAX);
        const auto res = EvalScript(GlobalConfig::GetConfig(),
                   consensus,
                   source->GetToken(),
                   stack,
                   script,
                   flags,
                   sig_checker);

        return bsv::get_raw_eval_script(res);
    }

    ScriptError execute_impl(const CScript& script,
                             const bool consensus,
                             const unsigned int flags)
    {
        BaseSignatureChecker sig_checker;
        return execute_impl(script, consensus, flags, sig_checker);
    }
}

ScriptError bsv::execute(const std::span<const uint8_t> script,
                         const bool consensus,
                         const unsigned int flags)
{
    if(script.empty())
        throw std::runtime_error("script empty");

    return execute_impl(CScript(script.data(), script.data() + script.size()),
                        consensus,
                        flags);
}

ScriptError bsv::execute(const std::string& script,
                         bool consensus,
                         unsigned int flags)
{
    if(script.empty())
        throw std::runtime_error("script empty");

    return execute_impl(ParseScript(script), consensus, flags);
}

namespace
{
    ScriptError execute_impl(const CScript& script,
                             const bool consensus,
                             const unsigned int flags,
                             const CMutableTransaction& mtx,
                             const int index,
                             const int64_t amount)
    {
        const CTransaction tx(mtx);
        if(!tx.vin.empty() && !tx.vout.empty())
        {
            const Amount amt{amount};
            TransactionSignatureChecker sig_checker(&tx, index, amt);
            return execute_impl(script, consensus, flags, sig_checker);
        }
        else
        {
            BaseSignatureChecker sig_checker;
            return execute_impl(script, consensus, flags, sig_checker);
        }
    }
}

ScriptError bsv::execute(const std::span<const uint8_t> script,
                         const bool consensus,
                         const unsigned int flags,
                         const std::span<const uint8_t> tx,
                         const int index,
                         const int64_t amount)
{
    if(script.empty())
        throw std::runtime_error("script empty");

    if(tx.empty())
        throw std::runtime_error("tx empty");

    const char* begin{reinterpret_cast<const char*>(tx.data())};
    const char* end{reinterpret_cast<const char*>(tx.data() + tx.size())};
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction mtx;
    tx_stream >> mtx;
    if(!tx_stream.empty())
    {
        throw std::runtime_error(
            "Unable to create a CMutableTransaction from supplied tx data");
    }

    return execute_impl(CScript(script.data(), script.data() + script.size()),
                        consensus,
                        flags,
                        mtx,
                        index,
                        amount);
}

ScriptError bsv::execute(const std::string& script,
                         const bool consensus,
                         const unsigned int flags,
                         const std::string& tx,
                         const int index,
                         const int64_t amount)
{
    if(script.empty())
        throw std::runtime_error("script empty");

    if(script.find_first_not_of(" /n/t/f") == std::string::npos)
        throw std::runtime_error("Control character in script");

    if(tx.empty())
        throw std::runtime_error("tx empty");

    // if(tx.find_first_not_of(" /n/t/f") != std::string::npos)
    //    throw std::runtime_error("Control character in tx");

    CMutableTransaction mtx;
    if(!DecodeHexTx(mtx, tx))
    {
        throw std::runtime_error("Unable to create a CMutableTransaction "
                                 "from supplied transaction hex");
    }

    return execute_impl(ParseScript(script), consensus, flags, mtx, index, amount);
}

namespace
{
    ScriptError verify_impl(const CScript& unlocking_script,
                            const CScript& locking_script,
                            const bool consensus,
                            const unsigned int flags,
                            BaseSignatureChecker& sig_checker,
                            std::atomic<malleability::status>& malleability)
    {
        auto source = task::CCancellationSource::Make();
        const auto ret = VerifyScript(GlobalConfig::GetConfig(),
                     consensus,
                     source->GetToken(),
                     unlocking_script,
                     locking_script,
                     flags,
                     sig_checker,
                     malleability);

        return bsv::get_raw_verify_script(ret);
    }

    ScriptError verify_impl(const CScript& unlocking_script,
                            const CScript& locking_script,
                            const bool consensus,
                            const unsigned int flags,
                            const CTransaction& tx,
                            const int index,
                            const int64_t amount,
                            std::atomic<malleability::status>& malleability)
    {
        if(!tx.vin.empty() && !tx.vout.empty())
        {
            const Amount amt{amount};
            TransactionSignatureChecker sig_checker(&tx, index, amt);
            return verify_impl(unlocking_script,
                               locking_script,
                               consensus,
                               flags,
                               sig_checker,
                               malleability);
        }
        else
        {
            BaseSignatureChecker sig_checker;
            return verify_impl(unlocking_script,
                               locking_script,
                               consensus,
                               flags,
                               sig_checker,
                               malleability);
        }
    }
}

ScriptError bsv::verify(const std::span<const uint8_t> unlocking_script,
                        const std::span<const uint8_t> locking_script,
                        const bool consensus,
                        const unsigned int flags,
                        const std::span<const uint8_t> tx,
                        const int index,
                        const int64_t amount)
{
    const char* begin{reinterpret_cast<const char*>(tx.data())};
    const char* end{reinterpret_cast<const char*>(tx.data() + tx.size())};
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction mtx;
    tx_stream >> mtx;

    if(!tx_stream.empty())
    {
        throw std::runtime_error("Unable to create a CMutableTransaction from "
                                 "supplied tx data");
    }

    const CTransaction ctx(mtx);
    std::atomic<malleability::status> ms {};
    return verify_impl(CScript(unlocking_script.data(), unlocking_script.data() + unlocking_script.size()),
                       CScript(locking_script.data(), locking_script.data() + locking_script.size()),
                       consensus,
                       flags,
                       ctx,
                       index,
                       amount,
                       ms);
}

ScriptError bsv::verify_extend(std::span<const uint8_t> extendedTX, int32_t blockHeight, bool consensus) {

    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty())
    {
        throw std::runtime_error("error serializing extended tx");
    }

    if (eTX.vutxo.size() != eTX.mtx.vin.size()) {
        throw std::runtime_error("inconsistent inputs size");
    }

    if (eTX.vutxo.empty() || eTX.mtx.vin.empty()) {
        return SCRIPT_ERR_UNKNOWN_ERROR;
    }

    const CTransaction ctx(eTX.mtx);
    std::atomic<malleability::status> ms {};
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const uint32_t flagsV2 = script_verification_flags_v2(lscript, blockHeight);

        ScriptError sERR = verify_impl(
            uscript,
            lscript,
            consensus, flagsV2, ctx,
            index,
            amount,
            ms);
        if (sERR != SCRIPT_ERR_OK) {
            return sERR;
        }
    }

    return SCRIPT_ERR_OK;
}

ScriptError bsv::verify_extend_full(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus) {

    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransactionExtended eTX;
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

    const CTransaction ctx(eTX.mtx);
    std::atomic<malleability::status> ms{};
    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const int32_t utxoHeight{ utxoHeights[index] };
        const uint32_t flags = bsv::script_verification_flags(utxoHeight, blockHeight, consensus);

        ScriptError sERR = verify_impl(
            uscript,
            lscript,
            consensus, flags, ctx,
            index,
            amount,
            ms);
        if (sERR != SCRIPT_ERR_OK) {
            return sERR;
        }
    }

    return SCRIPT_ERR_OK;
}