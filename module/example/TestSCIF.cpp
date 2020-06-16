/// Use it as an example how to add a example

#include <iostream>
#include <sstream>
#include <ScriptEngineIF.h>

int main(int argc, char * argv[]){

    const uint8_t direct[] = {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    bool concensus(true);
    unsigned int scriptsflags (0);
    if(ScriptEngineIF::executeScript(direct,concensus, scriptsflags, std::string(),0,0) == SCRIPT_ERR_OK)
        std::cout << "Sucessful script execution" << std::endl;


    std::string scriptString("0x00 0x6b 0x54 0x55 0x93 0x59 0x87");

    if(ScriptEngineIF::executeScript(scriptString,concensus, scriptsflags, std::string(),0,0) == SCRIPT_ERR_OK )
        std::cout << "Successful script execution from string" << std::endl;


    std::string scriptHashExample ("'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
    if(ScriptEngineIF::executeScript(scriptHashExample,concensus, scriptsflags,std::string(),0,0) == SCRIPT_ERR_OK)
        std::cout << "Successful script execution from string with a hash value" << std::endl;


    std::string scriptHash ("'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
    if(ScriptEngineIF::executeScript(scriptHash,concensus, scriptsflags,std::string(),0,0)  == SCRIPT_ERR_OK )
        std::cout << "Successful script execution from string with a hash value & op codes" << std::endl;

        
    std::string scriptVal = ScriptEngineIF::formatScript(scriptHash) ; 
    try{
        if(ScriptEngineIF::executeScript(scriptVal,concensus, scriptsflags,std::string(),0,0) == SCRIPT_ERR_OK )
            std::cout << "Successful script execution from string constructed from FormatScript" << std::endl;
    }catch(std::exception& e){
        std::cout << e.what() << " with script constructed from FormatScript " << std::endl;
    }

    std::cout << ScriptEngineIF::formatScript(scriptHash) << std::endl;
        
    return 0;
}
