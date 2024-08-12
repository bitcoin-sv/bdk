#ifndef __INTERPRETER_CGO_H__
#define __INTERPRETER_CGO_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

///!  Version of C++ core Script Engine SDK
int VersionMajor();
int VersionMinor();
int VersionPatch();
int VersionPatch();
const char * VersionString();

int cgo_execute(const char* scriptPtr, int scriptLen, bool consensus, unsigned int flags);


#ifdef __cplusplus
}
#endif

#endif /* __INTERPRETER_CGO_H__ */
