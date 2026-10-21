#ifndef BDK_GOLANG_SHA256_H
#define BDK_GOLANG_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The SHA-256 backend BDK hashes with, e.g. "shani,shani(2way)",
 * "arm_shani,arm_shani(2way)" or "standard". BDK selects the backend while the
 * program is loaded, from the environment it starts with; this only reports it.
 * The string has static storage; do not free it.
 */
const char* cgo_sha256_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* BDK_GOLANG_SHA256_H */
