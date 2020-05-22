/// Use it as an example how to add a example

#include <iostream>
#include <sstream>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <taskcancellation.h>
#include <config.h>

int main(int argc, char * argv[]){
    std::cout << "Test starting" << std::endl;
    static const uint8_t direct[] = {1, 0x5a};
    //static const uint8_t direct[] = {0x81, 0x82, OP_ADD, 0x83, OP_EQUAL, OP_CHECK};
    CScript scr;
    ScriptError err;
    LimitedStack directStack(UINT32_MAX);
    auto source = task::CCancellationSource::Make();
    const GlobalConfig& testConfig = GlobalConfig::GetConfig();

    auto res =
        EvalScript(
            testConfig, true,
            source->GetToken(),
            directStack,
            CScript(&direct[0], &direct[sizeof(direct)]),
            SCRIPT_VERIFY_P2SH,
            BaseSignatureChecker(),
            &err);

    return 0;
}