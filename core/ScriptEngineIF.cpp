#include <ScriptEngineIF.h>

#include <base58.h>
#include <chainparams.h>
#include <config.h>
#include <core_io.h>
#include <ecc_guard.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <taskcancellation.h>

ScriptError ScriptEngineIF::executeScript(const bsv::span<const uint8_t> script,
                                          const bool consensus,
                                          const unsigned int scriptflag,
                                          const std::string& txhex,
                                          const int vinIndex,
                                          const int64_t amount)
{
    ECCVerifyHandle verifyHandle;
    ecc_guard guard;

    CMutableTransaction mtx;
    std::unique_ptr<BaseSignatureChecker> sigCheck(new BaseSignatureChecker());
    if(txhex.empty() &&
       txhex.find_first_not_of(" /n/t/f") != std::string::npos)
    {
        if(!DecodeHexTx(mtx, txhex))
        {
            throw std::runtime_error("Unable to create a CMutableTransaction "
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
               CScript(script.begin(), script.end()), scriptflag,
               *(sigCheck.get()), &err);
    return err;
}

ScriptError ScriptEngineIF::executeScript(const std::string& inputScript,
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

    ECCVerifyHandle verifyHandle;
    ecc_guard guard;

    try
    {
        CScript in = ParseScript(inputScript);
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
        LimitedStack directStack(UINT32_MAX);
        ScriptError err;
        EvalScript(testConfig, consensus, source->GetToken(), directStack, in,
                   scriptflag, *(sigCheck.get()), &err);
        return err;
    }
    catch(std::exception& e)
    {
        throw std::runtime_error(e.what());
    }
}

ScriptError ScriptEngineIF::verifyScript(const std::string& scriptsig,
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

bool ScriptEngineIF::verifyScript(const bsv::span<const uint8_t>)
{
    return false;
}

std::string ScriptEngineIF::formatScript(const std::string& inputScript)
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

