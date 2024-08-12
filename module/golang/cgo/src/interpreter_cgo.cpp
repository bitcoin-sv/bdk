#include <version_go.h>
#include <interpreter_cgo.h>
#include <interpreter_sesdk.hpp>

int VersionMajor() { return SESDK_GOLANG_VERSION_MAJOR; }
int VersionMinor() { return SESDK_GOLANG_VERSION_MINOR; }
int VersionPatch() { return SESDK_GOLANG_VERSION_PATCH; }

const char * VersionString() {
    return SESDK_GOLANG_VERSION_STRING.c_str();
}