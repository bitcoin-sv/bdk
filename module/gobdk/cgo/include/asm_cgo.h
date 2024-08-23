#ifndef __ASM_CGO_H__
#define __ASM_CGO_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cgo_from_asm take input as a string of script in assembler language
 * give output as the script binary.
 * 
 * Client code must delete the returned array
 */
const char* cgo_from_asm(const char* asmPtr, int asmLen, int* scriptLen);

/**
 * cgo_to_asm take input as the script binary
 * give output as a string of script in assembler language.
 * 
 * The return string is a C-String null terminated
 * 
* Client code must delete the returned array
 */
const char* cgo_to_asm(const char* scriptPtr, int scriptLen);

#ifdef __cplusplus
}
#endif

#endif /* __ASM_CGO_H__ */
