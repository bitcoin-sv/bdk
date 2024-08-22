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

/**
 *  cgo_execute executes the script without verifying. Useful for simple script playing
 */
int cgo_execute_no_verify(const char* scriptPtr, int scriptLen,
                          bool consensus, unsigned int flags);

/**
 *  cgo_execute executes the script with verification.
 */
int cgo_execute(const char* scriptPtr, int scriptLen,
                bool consensus,
                unsigned int flags,
                const char* txPtr, int txLen,
                int index,
                unsigned long long amount);


/**
 *  cgo_verify verifies the locking and unlocking scripts
 */
int cgo_verify(const char* uScriptPtr, int uScriptLen,
               const char* lScriptPtr, int lScriptLen,
               bool consensus,
               unsigned int flags,
               const char* txPtr, int txLen,
               int index,
               unsigned long long amount);

#ifdef __cplusplus
}
#endif

#endif /* __INTERPRETER_CGO_H__ */
