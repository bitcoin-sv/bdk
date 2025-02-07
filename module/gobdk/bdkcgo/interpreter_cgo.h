#ifndef __INTERPRETER_CGO_H__
#define __INTERPRETER_CGO_H__

#include <stdbool.h>
#include <stdint.h>

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
 * This method calculates the flags based on the era of the protocol (block height)
 */
uint32_t cgo_script_verification_flags(const char* lScriptPtr, int lScriptLen, bool isChronicle);

/**
 * cgo_script_verification_flags_v2 calculates the flags to be used when verifying script
 * This method calculates the flags based on the block height
 */
uint32_t cgo_script_verification_flags_v2(const char* lScriptPtr, int lScriptLen, int32_t blockHeight);


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


/**
 *  cgo_verify_extend verifies the extended transaction
 *  It iterates through all the locking and unlocking scripts to verifies
 *  It return when the first error encountered
 */
int cgo_verify_extend(const char* extendedTxPtr, int extendedTxLen, int32_t blockHeight, bool consensus);

#ifdef __cplusplus
}
#endif

#endif /* __INTERPRETER_CGO_H__ */
