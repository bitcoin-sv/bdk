#include <ScriptEngineIF.h>

#include <script/interpreter.h>
#include <script/script.h>
#include <taskcancellation.h>
#include <config.h>
#include <chainparams.h>
#include <base58.h>
#include <core_io.h>
#include <ecc_guard.h>

namespace ScriptEngineIF
{
    ScriptError executeScript(const bsv::span<const uint8_t> script,const bool& consensus, const unsigned int& scriptflag,const std::string& txhex, const int& vinIndex, const int64_t& amount )
    {
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();

        ECCVerifyHandle verifyHandle;
        ecc_guard guard;
        
        CMutableTransaction mtx;
        std::unique_ptr<BaseSignatureChecker> sigCheck ( new BaseSignatureChecker() ); 
        if (txhex.length () > 0 && txhex.find_first_not_of(" /n/t/f" ) != std::string::npos){
            if (!DecodeHexTx(mtx, txhex) ){
                throw std::runtime_error ("Unable to create a CMutableTransaction from supplied transaction hex" );  
            }
        }
            
        CTransaction tx(mtx); 
        if ( !mtx.vin.empty() && !mtx.vout.empty() ){
           
            Amount a(amount);
            sigCheck.reset ( new TransactionSignatureChecker(&tx, vinIndex,a));
        }
            
        EvalScript(
            testConfig, consensus,
            source->GetToken(),
            directStack,
            CScript(script.begin(), script.end()),
            scriptflag,
            *(sigCheck.get()),
            &err);

        return err;
    }

    
    ScriptError executeScript(const std::string& inputScript, const bool& consensus, const unsigned int& scriptflag, const std::string& txhex,const int& vinIndex, const int64_t& amount){
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        
        if(inputScript.length() == 0 || inputScript.find_first_not_of (" /n/t/f") == std::string::npos){
            throw std::runtime_error ("No script provided to evalutate in ScirptEngine::executeScript");
        }
        
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();

        ECCVerifyHandle verifyHandle;
        ecc_guard guard;
        
        try{
            CScript in = ParseScript(inputScript);
            CMutableTransaction mtx; 
            std::unique_ptr<BaseSignatureChecker> sigCheck ( new BaseSignatureChecker() ); 
          
            if (txhex.length () > 0 && txhex.find_first_not_of(" /n/t/f" ) != std::string::npos){
                if (!DecodeHexTx(mtx, txhex) ){
                    throw std::runtime_error ("Unable to create a CMutableTransaction from supplied transaction hex" ); 
                } 
            }
            
            CTransaction tx(mtx); 
            if ( !mtx.vin.empty() && !mtx.vout.empty() ){
               
                Amount a(amount);
                sigCheck.reset ( new TransactionSignatureChecker(&tx, vinIndex,a));
            }
            
            EvalScript(
                testConfig, consensus,
                source->GetToken(),
                directStack,
                in,
                scriptflag,
                *(sigCheck.get()),
                &err);
                
        }catch(std::exception& e){
            throw std::runtime_error(e.what()); 
        }
        return err;
    }
    
    
    ScriptError verifyScript(const std::string& scriptsig,const std::string& scriptpubkey, const bool& consensus,const unsigned int& scriptflag, const std::string& txhex, const int& vinIndex , const int64_t& amount){
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();
        
        if(scriptsig.length() == 0 || scriptsig.find_first_not_of (" /n/t/f") == std::string::npos){
            throw std::runtime_error ("No script sig provided to evalutate in ScirptEngine::executeScript");
        }
        
        if(scriptpubkey.length() == 0 || scriptpubkey.find_first_not_of (" /n/t/f") == std::string::npos){
            throw std::runtime_error ("No script pubkey provided to evalutate in ScirptEngine::executeScript");
        }
        
        ECCVerifyHandle verifyHandle;
        ecc_guard guard;
        
        try{
            CScript inSig = ParseScript(scriptsig);
            CScript inPubkey = ParseScript(scriptpubkey);
            
            CMutableTransaction mtx; 
            std::unique_ptr<BaseSignatureChecker> sigCheck ( new BaseSignatureChecker() ); 
          
            if (txhex.length () > 0 && txhex.find_first_not_of(" /n/t/f" ) != std::string::npos){
                if (!DecodeHexTx(mtx, txhex) ){
                    throw std::runtime_error ("Unable to create a CMutableTransaction from supplied transaction hex" );  
                }
            }
            
            CTransaction tx(mtx); 
            if ( !mtx.vin.empty() && !mtx.vout.empty() ){
               
                Amount a(amount);
                sigCheck.reset ( new TransactionSignatureChecker(&tx, vinIndex,a));
            }
            
            VerifyScript(
                testConfig, consensus,
                source->GetToken(),
                inSig,
                inPubkey,
                scriptflag,
                *(sigCheck.get()),
                &err);

        }catch(std::exception& e){
            throw std::runtime_error(e.what()); 
        }

        return err;
    }


    bool verifyScript(bsv::span<const uint8_t>){
        return false;
    }

    
    
    std::string formatScript(const std::string& inputScript){
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();
        
        if(inputScript.length() == 0 || inputScript.find_first_not_of (" /n/t/f") == std::string::npos){
            throw std::runtime_error ("No script provided to formatScript in ScirptEngine::formatScript");
        }
        try{
            CScript in = ParseScript(inputScript);
            return FormatScript(in);
        }catch(std::exception& e){
            throw std::runtime_error(e.what()); 
        }
    }
}

