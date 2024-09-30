#include <interpreter_bdk.hpp>

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


// This method of flags calculation was replicated from $BSV/src/validation.cpp::GetBlockScriptFlags in Chronicle release 1.2.0
// The version implemented here has been slightly modified to adapt to the context of independant library
uint32_t bsv::script_verification_flags_v2(const std::span<const uint8_t> locking_script, int32_t blockHeight) {
    const Config& config = GlobalConfig::GetConfig();
    const Consensus::Params& consensusparams = config.GetChainParams().GetConsensus();
    uint32_t flags = SCRIPT_VERIFY_NONE;

    // We check if the utxo is P2SH by the content of the locking script, rather
    // than by the block height of the utxo
    try {
        if (IsP2SH(locking_script)) {
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

