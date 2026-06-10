#ifndef BDKFFI_ALLOC_H
#define BDKFFI_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Frees every caller-owned buffer returned by the bdkffi C ABI.
 */
void bdkffi_free(void* p);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_ALLOC_H */
