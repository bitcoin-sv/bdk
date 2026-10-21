#ifndef BDKFFI_SHA256_H
#define BDKFFI_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

/* The SHA-256 backend BDK hashes with, e.g. "shani,shani(2way)",
 * "arm_shani,arm_shani(2way)" or "standard". BDK selects the backend while the
 * program is loaded, from the environment it starts with; this only reports it.
 * Static storage: do not free. */
const char* bdkffi_sha256_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_SHA256_H */
