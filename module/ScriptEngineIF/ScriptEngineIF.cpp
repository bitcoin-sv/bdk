#include <ScriptEngineIF.h>

#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <taskcancellation.h>
#include <config.h>
#include <core_io.h>

namespace ScriptEngineIF
{
    bool executeScript(const bsv::span<const uint8_t> script)
    {
        CScript scr;
        ScriptError err;
        LimitedStack directStack(UINT32_MAX);
        auto source = task::CCancellationSource::Make();
        const GlobalConfig& testConfig = GlobalConfig::GetConfig();

        auto res = EvalScript(
            testConfig, true,
            source->GetToken(),
            directStack,
            CScript(script.begin(), script.end()),
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
        
        try{
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
        }catch(std::exception& e){
            throw std::runtime_error(e.what()); 
        }
        return false;
    }
    bool verifyScript(const std::string& inputScript){
        return false; 
    }
 
}

