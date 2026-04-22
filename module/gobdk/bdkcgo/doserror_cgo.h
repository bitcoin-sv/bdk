#ifndef __DOSERROR_CGO_H__
#define __DOSERROR_CGO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DoSError_t mirrors bsv::DoSError_t from core/doserror.hpp.
 * Explicit values guarantee 1:1 correspondence — the bridge static_cast's between them.
 */
typedef enum {
    DOS_ERR_OK                       = 0,
    DOS_ERR_NULL_PREVOUT             = 1,
    DOS_ERR_P2SH_OUTPUT_POST_GENESIS = 2,
    DOS_ERR_SIGOPS_CONSENSUS         = 3,
    DOS_ERR_SIGOPS_POLICY            = 4,
    DOS_ERR_NOT_FREE_CONSOLIDATION   = 5,
    DOS_ERR_NOT_STANDARD             = 6,
    DOS_ERR_COUNT                    = 7
} DoSError_t;

/**
 * dos_error_string delegates to bsv::DoSErrorString().
 * Returned string is a literal constant — callers must NOT free it.
 */
const char* dos_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* __DOSERROR_CGO_H__ */
