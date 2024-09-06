#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <sstream>

#include <script.h>
#include <core/assembler.h>
#include <cgo/asm_cgo.h>

const char* cgo_from_asm(const char* asmPtr, int asmLen, int* scriptLen)
{
    char* scriptPtr = nullptr;
    try {
        const std::string asmStr(asmPtr, asmLen);
        auto script = bsv::from_asm(asmStr);
        *scriptLen = script.size();
        scriptPtr = (char*)malloc((*scriptLen) * sizeof(char));
        std::memcpy(scriptPtr, script.data(), *scriptLen);
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ <<std::endl;
        std::string result = ss.str();
        scriptPtr = new char[result.size() + 1];
        std::strcpy(scriptPtr, result.c_str());
    }

    return scriptPtr;
}

const char* cgo_to_asm(const char* scriptPtr, int scriptLen)
{
    char* asmPtr = nullptr;
    try {
        const uint8_t* p = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
        const std::span<const uint8_t> script(p, scriptLen);
        auto asmStr = bsv::to_asm(script);
        auto asmLen = asmStr.size()+1; // extra null terminator in C-String 

        asmPtr = (char*)malloc(asmLen);
        std::memcpy(asmPtr, asmStr.c_str(), asmLen);
        asmPtr[asmStr.size()] = '\0';
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss <<  "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ <<"    at " <<__func__ <<std::endl;
        std::string result = ss.str();
        asmPtr = new char[result.size() + 1];
        std::strcpy(asmPtr, result.c_str());
    }

    return asmPtr;
}
