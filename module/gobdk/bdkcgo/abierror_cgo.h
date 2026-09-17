#ifndef __ABIERROR_CGO_H__
#define __ABIERROR_CGO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AbiError_t mirrors bsv::AbiError_t from core/abierror.h.
 * Explicit values guarantee 1:1 correspondence — the bridge static_cast's between them.
 *
 * These codes report a failure to express the call, never a verdict on the
 * transaction: a consumer must classify TX_ERR_DOMAIN_ABI as a local fault.
 */
typedef enum {
    ABI_ERR_OK                       = 0,
    ABI_ERR_LENGTH_NEGATIVE          = 1,
    ABI_ERR_LENGTH_NOT_REPRESENTABLE = 2,
    ABI_ERR_NULL_BUFFER              = 3,
    ABI_ERR_LENGTH_OVERFLOW          = 4,
    ABI_ERR_COUNT                    = 5
} AbiError_t;

/**
 * abi_error_string delegates to bsv::AbiErrorString().
 * Returned string is a literal constant — callers must NOT free it.
 */
const char* abi_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* __ABIERROR_CGO_H__ */
