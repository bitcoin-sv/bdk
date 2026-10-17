#ifndef BDKFFI_SHA256_H
#define BDKFFI_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

/* The SHA-256 backend BDK hashes with, e.g. "shani,shani(2way)",
 * "arm_shani,arm_shani(2way)" or "standard". Selects it first if no validator
 * has been constructed yet. Static storage: do not free. */
const char* bdkffi_sha256_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_SHA256_H */
