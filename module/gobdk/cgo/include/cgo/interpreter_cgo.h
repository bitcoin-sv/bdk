#ifndef __INTERPRETER_CGO_H__
#define __INTERPRETER_CGO_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * All code called from C/C++ did not handle exception.
 * We handle it here and return and 'absurd' value so golang layer can recognize
 * the returned result is an exception
 * 
 * For script evaluation/verification, if an exception was thrown, it will be handled
 * here, and return SCRIPT_ERR_ERROR_COUNT+1
 * 
 * For cgo_script_verification_flags, if an exception was thrown, it will be handled
 * here, and return SCRIPT_FLAG_LAST+1
 */

/**
 * cgo_script_verification_flags calculates the flags to be used when verifying script
 */
unsigned int cgo_script_verification_flags(const char* lScriptPtr, int lScriptLen, bool isChronicle);

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