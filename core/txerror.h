#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TxErrorDomain_t identifies which error domain a TxError code belongs to.
 */
// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
typedef enum {
    TX_ERR_DOMAIN_OK        = 0,  /* success, code is irrelevant */
    TX_ERR_DOMAIN_SCRIPT    = 1,  /* code is ScriptError_t (script execution failure) */
    TX_ERR_DOMAIN_DOS       = 2,  /* code is DoSError_t (transaction validation failure) */
    TX_ERR_DOMAIN_EXCEPTION = 3,  /* C++ exception caught at CGO boundary */
    TX_ERR_DOMAIN_ABI       = 4,  /* code is AbiError_t (the call could not be expressed) */
    TX_ERR_DOMAIN_CANCELLED = 5   /* the caller cancelled the call before it reached a verdict, code is 0 */
} TxErrorDomain_t;

/**
 * TxError is the unified return type for CTxValidator operations that can fail
 * in either the script or the DoS domain.
 * Passed by value (8 bytes). No heap allocation.
 *
 * Success:        { domain=TX_ERR_DOMAIN_OK,        code=0 }
 * Script failure: { domain=TX_ERR_DOMAIN_SCRIPT,    code=ScriptError_t value }
 * DoS failure:    { domain=TX_ERR_DOMAIN_DOS,        code=DoSError_t value }
 * Exception:      { domain=TX_ERR_DOMAIN_EXCEPTION, code=0 }
 * ABI failure:    { domain=TX_ERR_DOMAIN_ABI,       code=AbiError_t value }
 * Cancelled:      { domain=TX_ERR_DOMAIN_CANCELLED, code=0 }
 *
 * TX_ERR_DOMAIN_SCRIPT and TX_ERR_DOMAIN_DOS are verdicts about the transaction.
 * TX_ERR_DOMAIN_EXCEPTION and TX_ERR_DOMAIN_ABI are not: they report a failure of
 * this process to carry out the call, and a consumer must classify them as a local
 * fault rather than as an invalid transaction.
 *
 * TX_ERR_DOMAIN_CANCELLED is not a verdict either: the caller's cancellation token
 * fired before the call finished, so there is no answer about the transaction. Only
 * the cancellable overloads produce it. A token that fires after the last check
 * leaves the call to return its verdict as usual.
 */
typedef struct {
    int32_t domain;  /* TxErrorDomain_t */
    int32_t code;    /* error code within the domain */
} TxError;

#ifdef __cplusplus
}
#endif

/* C++ helpers — not visible from C */
#ifdef __cplusplus
#include <script/script_error.h>

namespace bsv {
    inline TxError TxErrorOk()                    { return { TX_ERR_DOMAIN_OK,        0 }; }
    inline TxError TxErrorScript(ScriptError e)   { return { TX_ERR_DOMAIN_SCRIPT,    static_cast<int32_t>(e) }; }
    inline TxError TxErrorDoS(int32_t e)          { return { TX_ERR_DOMAIN_DOS,        e }; }
    inline TxError TxErrorException()             { return { TX_ERR_DOMAIN_EXCEPTION, 0 }; }
    inline TxError TxErrorAbi(int32_t e)          { return { TX_ERR_DOMAIN_ABI,       e }; }
    inline TxError TxErrorCancelled()             { return { TX_ERR_DOMAIN_CANCELLED, 0 }; }
    inline bool    TxErrorIsOk(TxError r)         { return r.domain == TX_ERR_DOMAIN_OK; }
}
#endif

