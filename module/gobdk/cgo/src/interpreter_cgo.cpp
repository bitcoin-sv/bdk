#include <version_go.h>
#include <interpreter_cgo.h>
#include <interpreter_sesdk.hpp>

#include <iostream>

int VersionMajor() { return BDK_GOLANG_VERSION_MAJOR; }
int VersionMinor() { return BDK_GOLANG_VERSION_MINOR; }
int VersionPatch() { return BDK_GOLANG_VERSION_PATCH; }

const char * VersionString() {
    return BDK_GOLANG_VERSION_STRING.c_str();
}

unsigned int cgo_script_verification_flags(const char* lScriptPtr, int lScriptLen, bool isChronicle)
{
    const uint8_t* p = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
    const std::span<const uint8_t> lScript(p, lScriptLen);
    return bsv::script_verification_flags(lScript, isChronicle);
}

int cgo_execute_no_verify(const char* scriptPtr, int scriptLen, bool consensus, unsigned int flags)
{
    const uint8_t* p = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
    const std::span<const uint8_t> script(p, scriptLen);
    return bsv::execute(script, consensus, flags);
}

int cgo_execute(const char* scriptPtr, int scriptLen,
                bool consensus,
                unsigned int flags,
                const char* txPtr, int txLen,
                int index,
                unsigned long long amount)
{
    const uint8_t* pS = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
    const std::span<const uint8_t> script(pS, scriptLen);

    const uint8_t* pT = static_cast<const uint8_t*>(reinterpret_cast<const void*>(txPtr));
    const std::span<const uint8_t> tx(pT, txLen);


    return bsv::execute(script, consensus, flags, tx, index, amount);
}


int cgo_verify(const char* uScriptPtr, int uScriptLen,
               const char* lScriptPtr, int lScriptLen,
               bool consensus,
               unsigned int flags,
               const char* txPtr, int txLen,
               int index,
               unsigned long long amount)
{
    const uint8_t* pU = static_cast<const uint8_t*>(reinterpret_cast<const void*>(uScriptPtr));
    const std::span<const uint8_t> uScript(pU, uScriptLen);

    const uint8_t* pL = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
    const std::span<const uint8_t> lScript(pL, lScriptLen);

    const uint8_t* pT = static_cast<const uint8_t*>(reinterpret_cast<const void*>(txPtr));
    const std::span<const uint8_t> tx(pT, txLen);

    return bsv::verify(uScript, lScript, consensus, flags, tx, index, amount);
}
