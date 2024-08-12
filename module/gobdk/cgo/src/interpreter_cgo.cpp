#include <version_go.h>
#include <interpreter_cgo.h>
#include <interpreter_sesdk.hpp>

#include <iostream>

int VersionMajor() { return SESDK_GOLANG_VERSION_MAJOR; }
int VersionMinor() { return SESDK_GOLANG_VERSION_MINOR; }
int VersionPatch() { return SESDK_GOLANG_VERSION_PATCH; }

const char * VersionString() {
    return SESDK_GOLANG_VERSION_STRING.c_str();
}

int cgo_execute(const char* scriptPtr, int scriptLen, bool consensus, unsigned int flags)
{
    std::cout<<"Calling cggo_execute from C code"<<std::endl;
    const uint8_t* p = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
    const bsv::span<const uint8_t> script(p, scriptLen);
    return execute(script, consensus, flags);
}
