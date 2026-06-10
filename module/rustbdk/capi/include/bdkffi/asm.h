#ifndef BDKFFI_ASM_H
#define BDKFFI_ASM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Converts assembler text to script bytes.
 * Returns a malloc-owned buffer and writes its byte length to script_len.
 * Returns NULL and sets script_len to 0 on exception or allocation failure.
 */
char* bdkffi_from_asm(const char* asm_ptr, int asm_len, int* script_len);

/*
 * Converts script bytes to a malloc-owned, null-terminated assembler string.
 * Returns NULL on exception or allocation failure.
 */
char* bdkffi_to_asm(const char* script_ptr, int script_len);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_ASM_H */
