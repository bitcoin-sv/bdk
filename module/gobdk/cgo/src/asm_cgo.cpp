#include <assembler.h>
#include <asm_cgo.h>
#include <string>
#include <cstring>
#include <cstdlib>

#include <script.h>

const char* cgo_from_asm(const char* asmPtr, int asmLen, int* scriptLen)
{
    const std::string asmStr(asmPtr, asmLen);
    auto script = bsv::from_asm(asmStr);
    *scriptLen = script.size();
    char* scriptPtr = (char*)malloc((*scriptLen) * sizeof(char));
    std::memcpy(scriptPtr, script.data(), *scriptLen);
    return scriptPtr;
}

const char* cgo_to_asm(const char* scriptPtr, int scriptLen)
{
    const uint8_t* p = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
    const std::span<const uint8_t> script(p, scriptLen);
    auto asmStr = bsv::to_asm(script);
    auto asmLen = asmStr.size()+1; // extra null terminator in C-String 

    char* asmPtr = (char*)malloc(asmLen);
    std::memcpy(asmPtr, asmStr.c_str(), asmLen);
    asmPtr[asmStr.size()] = '\0';
    return asmPtr;
}
