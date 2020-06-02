#include <ScriptEngineIF.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <taskcancellation.h>
#include <config.h>
#include <core_io.h>


namespace ScriptEngineIF
{
    bool executeScript(const std::unique_ptr<unsigned char[]>& script, const size_t& scriptLen ){
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();

        auto res = EvalScript(
            testConfig, true,
            source->GetToken(),
            directStack,
            //CScript(&direct[0], &direct[sizeof(direct)]),
            CScript(&script.get()[0],&script.get()[scriptLen]),
            SCRIPT_VERIFY_P2SH,
            BaseSignatureChecker(),
            &err);
            
        if(res.has_value()){
            return res.value();
        }
        return false;
    }
    bool verifyScript(const std::unique_ptr<unsigned char[]>& script, const size_t& scriptLen ){
        return false;
    }
    
    
    bool executeScript(const std::string& inputScript){
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();
        
        CScript in = ParseScript(inputScript);
        auto res = EvalScript(
            testConfig, true,
            source->GetToken(),
            directStack,
            in,
            SCRIPT_VERIFY_P2SH,
            BaseSignatureChecker(),
            &err);
            
        if(res.has_value()){
            return res.value();
        }
        return false;
    }
    bool verifyScript(const std::string& inputScript){
        return false; 
    }
 
}

