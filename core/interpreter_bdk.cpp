#include <interpreter_bdk.hpp>
#include <extendedTx.hpp>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <protocol_era.h>
#include <script/script.h>
#include <core_io.h>
#include <script/interpreter.h>
#include <script/script_flags.h>

#include <stdexcept>

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
        SelectParams(chainNetwork); // throw exception if wrong network string
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
bool is_p2sh_activated(const Config& config, int32_t blockHeight) {
    // We detect the chain using the genesis height on the consensus of the chain params
    const Consensus::Params& consensusparams = config.GetChainParams().GetConsensus();
    const int32_t genesisHeight = consensusparams.genesisHeight;
    const bool isMainNet = (genesisHeight == GENESIS_ACTIVATION_MAIN);
    const bool isTestNet = (genesisHeight == GENESIS_ACTIVATION_TESTNET);
    const bool isSTNNet = (genesisHeight == GENESIS_ACTIVATION_STN);
    const bool isRegTest = (genesisHeight == GENESIS_ACTIVATION_REGTEST);

    // If non of the network was detected, that mean something is wrong
    if (!(isMainNet || isTestNet || isSTNNet || isRegTest)) {
        std::stringstream ss;
        ss <<  "EXCEPTION unable to detect chain : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ <<std::endl;
        throw std::runtime_error(ss.str());
    }

    // P2SH_ACTIVATION_TIME = 1333234914; // 31 March 2012 23:01:54
    // Our onchain analysis provide
    //     Mainnet block 173799  2012-03-31 23:01:54
    //     Mainnet block 173800  2012-03-31 23:24:39
    //     Testnet block    513  2011-02-03 15:52:39
    //     Testnet block    514  2012-05-24 14:54:10
    // There are only two cases where P2SH is not activated
    if (isMainNet && blockHeight<173800)
        return false;
    if (isTestNet && blockHeight<514)
        return false;

    // For STN or regtest, P2SH is always activated as they all started after P2SH_ACTIVATION_TIME
    return true;
}

// This method of flags calculation was replicated from $BSV/src/validation.cpp::GetBlockScriptFlags in Chronicle release 1.2.0
// The version implemented here has been slightly modified to adapt to the context of independant library
uint32_t bsv::script_verification_flags_v2(const std::span<const uint8_t> locking_script, int32_t blockHeight) {
    const Config& config = GlobalConfig::GetConfig();
    const Consensus::Params& consensusparams = config.GetChainParams().GetConsensus();
    uint32_t flags = SCRIPT_VERIFY_NONE;

    // We check if the utxo is P2SH by the content of the locking script, rather
    // than by the block height of the utxo
    try {
        if (is_p2sh_activated(config, blockHeight) && IsP2SH(locking_script)) {
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
        flags &= ~SCRIPT_VERIFY_LOW_S;
        flags &= ~SCRIPT_VERIFY_SIGPUSHONLY;
    }

    // Trying to get the utxoEra using a combination of methods in $BSV/src/bitcoin-tx.cpp
    // To get the InputScriptVerifyFlags from the utxo
    const ProtocolEra ActiveEra{ GetProtocolEra(config, blockHeight) };
    const ProtocolEra utxoEra{ IsP2SH(locking_script) ? ProtocolEra::PreGenesis : ActiveEra };
    flags |= InputScriptVerifyFlags(ActiveEra, utxoEra);
    return flags;
}


// This method of flags calculation was found in $BSV/src/bitcoin-tx.cpp in Chronicle release 1.2.0
// It is calculated based on the locking script and the boolean isPostChronical
// If the node parameter -genesis is set to true, then the argument isPostChronical is false
// Otherwise, isPostChronical is true
uint32_t bsv::script_verification_flags(const std::span<const uint8_t> locking_script, const bool isPostChronical){
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

namespace
{
    ScriptError execute_impl(const CScript& script,
                             const bool consensus,
                             const unsigned int flags,
                             BaseSignatureChecker& sig_checker)
    {
        auto source = task::CCancellationSource::Make();
        LimitedStack stack(UINT32_MAX);
        ScriptError err{};
        EvalScript(GlobalConfig::GetConfig(),
                   consensus,
                   source->GetToken(),
                   stack,
                   script,
                   flags,
                   sig_checker,
                   &err);
        return err;
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
                            BaseSignatureChecker& sig_checker)
    {
        auto source = task::CCancellationSource::Make();
        ScriptError err{};
        VerifyScript(GlobalConfig::GetConfig(),
                     consensus,
                     source->GetToken(),
                     unlocking_script,
                     locking_script,
                     flags,
                     sig_checker,
                     &err);
        return err;
    }

    ScriptError verify_impl(const CScript& unlocking_script,
                            const CScript& locking_script,
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
            return verify_impl(unlocking_script,
                               locking_script,
                               consensus,
                               flags,
                               sig_checker);
        }
        else
        {
            BaseSignatureChecker sig_checker;
            return verify_impl(unlocking_script,
                               locking_script,
                               consensus,
                               flags,
                               sig_checker);
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

    return verify_impl(CScript(unlocking_script.data(), unlocking_script.data() + unlocking_script.size()),
                       CScript(locking_script.data(), locking_script.data() + locking_script.size()),
                       consensus,
                       flags,
                       mtx,
                       index,
                       amount);
}

ScriptError bsv::verify_extend(std::span<const uint8_t> extendedTX, int32_t blockHeight) {

    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty())
    {
        throw std::runtime_error("error serializing extended tx");
    }

    for (size_t index = 0; index < eTX.vutxo.size(); ++index) {
        const uint64_t amount = eTX.vutxo[index].nValue.GetSatoshis();
        const CScript& lscript = eTX.vutxo[index].scriptPubKey; //   locking script
        const CScript& uscript = eTX.mtx.vin[index].scriptSig;  // unlocking script

        const uint32_t flagsV2 = script_verification_flags_v2(lscript, blockHeight);

        ScriptError sERR = verify_impl(
            uscript,
            lscript,
            true, flagsV2, eTX.mtx,
            index,
            amount);
        if (sERR != SCRIPT_ERR_OK) {
            return sERR;
        }
    }

    return SCRIPT_ERR_OK;
}