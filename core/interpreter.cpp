#include <interpreter.h>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <core_io.h>
#include <ecc_guard.h>
#include <script/interpreter.h>

#include <stdexcept>

using namespace std;

using unique_sig_checker = unique_ptr<BaseSignatureChecker>;

namespace
{
    unique_sig_checker make_unique_sig_checker() { return make_unique<BaseSignatureChecker>(); }

    unique_sig_checker make_unique_sig_checker(const CTransaction& tx, const int vinIndex, const int64_t a)
    {
        Amount amount{a};
        return make_unique<TransactionSignatureChecker>(&tx, vinIndex, amount);
    }

    ScriptError evaluate_impl(const CScript& script,
                              const bool consensus,
                              const unsigned int scriptflag,
                              BaseSignatureChecker* sigCheck)
    {
        ECCVerifyHandle verify_handle;
        ecc_guard guard;
        auto source = task::CCancellationSource::Make();
        LimitedStack direct_stack(UINT32_MAX);
        ScriptError err;
        EvalScript(GlobalConfig::GetConfig(), consensus, source->GetToken(), direct_stack, script, scriptflag,
                   *sigCheck, &err);
        return err;
    }

    ScriptError evaluate_impl(const CScript& script,
                              const bool consensus,
                              const unsigned int scriptflag,
                              const std::string& txhex,
                              const int vinIndex,
                              const int64_t amount)
    {
        CMutableTransaction mtx;

        if(!txhex.empty() && txhex.find_first_not_of(" /n/t/f") != std::string::npos)
        {
            if(!DecodeHexTx(mtx, txhex))
            {
                throw std::runtime_error("Unable to create a CMutableTransaction "
                                         "from supplied transaction hex");
            }
        }

        unique_sig_checker sig_check{make_unique_sig_checker()};
        CTransaction tx(mtx);
        if(!mtx.vin.empty() && !mtx.vout.empty())
        {
            sig_check = make_unique_sig_checker(tx, vinIndex, amount);
        }

        return evaluate_impl(script, consensus, scriptflag, sig_check.get());
    }
}

ScriptError bsv::evaluate(const bsv::span<const uint8_t> script,
                          const bool consensus,
                          const unsigned int scriptflag,
                          const std::string& txhex,
                          const int vinIndex,
                          const int64_t amount)
{
    return evaluate_impl(CScript{script.begin(), script.end()}, consensus, scriptflag, txhex, vinIndex,
                         amount);
}

ScriptError bsv::evaluate(const std::string& script,
                          const bool consensus,
                          const unsigned int scriptflag,
                          const std::string& txhex,
                          const int vinIndex,
                          const int64_t amount)
{
    if(script.empty())
        throw std::runtime_error("empty script");

    if(script.find_first_not_of(" /n/t/f") == std::string::npos)
        throw std::runtime_error("Control character in script");

    return evaluate_impl(ParseScript(script), consensus, scriptflag, txhex, vinIndex, amount);
}
