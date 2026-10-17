#ifndef BDK_GOLANG_SHA256_H
#define BDK_GOLANG_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The SHA-256 backend BDK hashes with, e.g. "shani,shani(2way)",
 * "arm_shani,arm_shani(2way)" or "standard". Selects it first if no validator
 * has been constructed yet. The string has static storage; do not free it.
 */
const char* cgo_sha256_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* BDK_GOLANG_SHA256_H */
