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
#include <taskcancellation.h>

#include <stdexcept>
#include <iostream>
#include <chrono>

using namespace std;

std::optional<std::variant<ScriptError, malleability::status>> EvalScript(
    const Config& config,
    const bool consensus,
    const task::CCancellationToken& token,
    LimitedStack& stack,
    const CScript& script,
    const uint32_t flags,
    const BaseSignatureChecker& checker)
{
    const eval_script_params params{make_eval_script_params(config, flags, consensus)};
    return EvalScript(params,
                        token,
                        stack,
                        script,
                        flags,
                        checker);
}

std::optional<std::variant<ScriptError, malleability::status>> EvalScript(
    const Config& config,
    const bool consensus,
    const task::CCancellationToken& token,
    LimitedStack& stack,
    const CScript& script,
    const uint32_t flags,
    const BaseSignatureChecker& checker,
    LimitedStack& altstack,
    long& ipc,
    std::vector<bool>& vfExec,
    std::vector<bool>& vfElse)
{
    const eval_script_params params{make_eval_script_params(config, flags, consensus)};
    return EvalScript(params,
                      token,
                      stack,
                      script,
                      flags,
                      checker,
                      altstack,
                      ipc,
                      vfExec,
                      vfElse);
}

std::optional<std::pair<bool, ScriptError>> VerifyScript(
    const Config& config,
    const bool consensus,
    const task::CCancellationToken& token,
    const CScript& scriptSig,
    const CScript& scriptPubKey,
    const uint32_t flags,       
    const BaseSignatureChecker& checker, 
    std::atomic<malleability::status>& malleability)
{
    const verify_script_params params{make_verify_script_params(config, flags, consensus)};
    return VerifyScript(params,
                        token,
                        scriptSig,
                        scriptPubKey,
                        flags,
                        checker,
                        malleability);
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

        const verify_script_params veriParams{make_verify_script_params(GlobalConfig::GetConfig(), flags, consensus)};
        const auto ret = VerifyScript(veriParams,
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