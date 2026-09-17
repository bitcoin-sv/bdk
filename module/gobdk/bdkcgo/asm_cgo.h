#ifndef __ASM_CGO_H__
#define __ASM_CGO_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cgo_from_asm_v2 take input as a string of script in assembler language
 * give output as the script binary.
 *
 * scriptLen is checked for NULL first and then set to 0, so that it is defined on
 * every exit. A NULL scriptLen returns NULL and writes nothing. On failure the
 * returned blob is the diagnostic message and scriptLen stays 0.
 *
 * The returned array is allocated with malloc.
 * Client code must free the returned array
 *
 * The _v2 suffix is this surface's ABI generation marker: a consumer built against
 * an archive predating the uint64_t length ABI fails to link instead of silently
 * reading a narrowed length.
 */
const char* cgo_from_asm_v2(const char* asmPtr, uint64_t asmLen, uint64_t* scriptLen);

/**
 * cgo_to_asm_v2 take input as the script binary
 * give output as a string of script in assembler language.
 *
 * The return string is a C-String null terminated
 *
 * The returned array is allocated with malloc.
 * Client code must free the returned array
 */
const char* cgo_to_asm_v2(const char* scriptPtr, uint64_t scriptLen);

#ifdef __cplusplus
}
#endif

#endif /* __ASM_CGO_H__ */
