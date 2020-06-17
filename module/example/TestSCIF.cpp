/// Use it as an example how to add a example

#include <iostream>
#include <sstream>
#include <ScriptEngineIF.h>

int main(int argc, char * argv[]){

    const uint8_t direct[] = {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    
    if(ScriptEngineIF::executeScript(direct))
        std::cout << "Sucessful script execution" << std::endl;
        
        
    std::string scriptString("0x00 0x6b 0x54 0x55 0x93 0x59 0x87");
    if(ScriptEngineIF::executeScript(scriptString))
        std::cout << "Successful script execution from string" << std::endl;
        
        
    std::string scriptHashExample ("'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
    if(ScriptEngineIF::executeScript(scriptHashExample))
        std::cout << "Successful script execution from string with a hash value" << std::endl;
        
        
    std::string scriptHash ("'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
    if(ScriptEngineIF::executeScript(scriptHash))
        std::cout << "Successful script execution from string with a hash value & op codes" << std::endl;
        
        
        
    return 0;
}
