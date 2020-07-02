#include <ScriptEngineIF.h>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <core_io.h>
#include <ecc_guard.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <taskcancellation.h>

namespace
{
    ScriptError evaluate_impl(const CScript& script,
                              const bool consensus,
                              const unsigned int scriptflag,
                              const std::string& txhex,
                              const int vinIndex,
                              const int64_t amount)
    {
        ECCVerifyHandle verifyHandle;
        ecc_guard guard;

        CMutableTransaction mtx;
        std::unique_ptr<BaseSignatureChecker> sigCheck(
            new BaseSignatureChecker());

        if(!txhex.empty() &&
           txhex.find_first_not_of(" /n/t/f") != std::string::npos)
        {
            if(!DecodeHexTx(mtx, txhex))
            {
                throw std::runtime_error(
                    "Unable to create a CMutableTransaction "
                    "from supplied transaction hex");
            }
        }

        CTransaction tx(mtx);
        if(!mtx.vin.empty() && !mtx.vout.empty())
        {

            Amount a(amount);
            sigCheck.reset(new TransactionSignatureChecker(&tx, vinIndex, a));
        }

        const GlobalConfig& testConfig = GlobalConfig::GetConfig();
        auto source = task::CCancellationSource::Make();
        LimitedStack directStack(UINT32_MAX);
        ScriptError err;
        EvalScript(testConfig, consensus, source->GetToken(), directStack,
                   script, scriptflag, *(sigCheck.get()), &err);
        return err;
    }
}

ScriptError bsv::evaluate(const bsv::span<const uint8_t> script,
                          const bool consensus,
                          const unsigned int scriptflag,
                          const std::string& txhex,
                          const int vinIndex,
                          const int64_t amount)
{
    return evaluate_impl(CScript{script.begin(), script.end()}, consensus,
                         scriptflag, txhex, vinIndex, amount);
}

ScriptError bsv::evaluate(const std::string& inputScript,
                          const bool consensus,
                          const unsigned int scriptflag,
                          const std::string& txhex,
                          const int vinIndex,
                          const int64_t amount)
{
    if(inputScript.empty() ||
       inputScript.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        throw std::runtime_error(
            "No script provided to evalutate in ScriptEngine::executeScript");
    }

    CScript script{ParseScript(inputScript)};
    return evaluate_impl(CScript{script.begin(), script.end()}, consensus,
                         scriptflag, txhex, vinIndex, amount);
}

ScriptError bsv::verifyScript(const std::string& scriptsig,
                                         const std::string& scriptpubkey,
                                         const bool consensus,
                                         const unsigned int scriptflag,
                                         const std::string& txhex,
                                         const int vinIndex,
                                         const int64_t amount)
{
    if(scriptsig.empty() ||
       scriptsig.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        throw std::runtime_error("No script sig provided to evalutate in "
                                 "ScirptEngine::executeScript");
    }

    if(scriptpubkey.length() == 0 ||
       scriptpubkey.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        throw std::runtime_error("No script pubkey provided to evalutate in "
                                 "ScirptEngine::executeScript");
    }

    ECCVerifyHandle verifyHandle;
    ecc_guard guard;

    try
    {
        CScript inSig = ParseScript(scriptsig);
        CScript inPubkey = ParseScript(scriptpubkey);

        CMutableTransaction mtx;
        std::unique_ptr<BaseSignatureChecker> sigCheck(
            new BaseSignatureChecker());

        if(txhex.length() > 0 &&
           txhex.find_first_not_of(" /n/t/f") != std::string::npos)
        {
            if(!DecodeHexTx(mtx, txhex))
            {
                throw std::runtime_error(
                    "Unable to create a CMutableTransaction from supplied "
                    "transaction hex");
            }
        }

        CTransaction tx(mtx);
        if(!mtx.vin.empty() && !mtx.vout.empty())
        {

            Amount a(amount);
            sigCheck.reset(new TransactionSignatureChecker(&tx, vinIndex, a));
        }

        const GlobalConfig& testConfig = GlobalConfig::GetConfig();
        auto source = task::CCancellationSource::Make();
        ScriptError err;
        VerifyScript(testConfig, consensus, source->GetToken(), inSig, inPubkey,
                     scriptflag, *(sigCheck.get()), &err);
        return err;
    }
    catch(std::exception& e)
    {
        throw std::runtime_error(e.what());
    }
}

bool bsv::verifyScript(const bsv::span<const uint8_t>)
{
    return false;
}

std::string bsv::formatScript(const std::string& inputScript)
{
    if(inputScript.empty() ||
       inputScript.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        throw std::runtime_error(
            "No script provided to formatScript in ScirptEngine::formatScript");
    }
    try
    {
        CScript in = ParseScript(inputScript);
        return FormatScript(in);
    }
    catch(std::exception& e)
    {
        throw std::runtime_error(e.what());
    }
}

