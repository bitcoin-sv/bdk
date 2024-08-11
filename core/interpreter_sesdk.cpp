#include <interpreter_sesdk.hpp>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <core_io.h>
#include <script/interpreter.h>

#include <stdexcept>

using namespace std;

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

ScriptError bsv::execute(const bsv::span<const uint8_t> script,
                         const bool consensus,
                         const unsigned int flags)
{
    if(script.empty())
        throw std::runtime_error("script empty");

    return execute_impl(CScript{script.begin(), script.end()},
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

ScriptError bsv::execute(const bsv::span<const uint8_t> script,
                         const bool consensus,
                         const unsigned int flags,
                         const bsv::span<const uint8_t> tx,
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

    return execute_impl(CScript{script.begin(), script.end()},
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

ScriptError bsv::verify(const bsv::span<const uint8_t> unlocking_script,
                        const bsv::span<const uint8_t> locking_script,
                        const bool consensus,
                        const unsigned int flags,
                        const bsv::span<const uint8_t> tx,
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

    return verify_impl(CScript{unlocking_script.begin(), unlocking_script.end()},
                       CScript{locking_script.begin(), locking_script.end()},
                       consensus,
                       flags,
                       mtx,
                       index,
                       amount);
}

