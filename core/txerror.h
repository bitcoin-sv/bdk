#ifndef __TXERROR_H__
#define __TXERROR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TxErrorDomain_t identifies which error domain a TxError code belongs to.
 */
typedef enum {
    TX_ERR_DOMAIN_OK        = 0,  /* success, code is irrelevant */
    TX_ERR_DOMAIN_SCRIPT    = 1,  /* code is ScriptError_t (script execution failure) */
    TX_ERR_DOMAIN_DOS       = 2,  /* code is DoSError_t (transaction validation failure) */
    TX_ERR_DOMAIN_EXCEPTION = 3   /* C++ exception caught at CGO boundary */
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
    inline bool    TxErrorIsOk(TxError r)         { return r.domain == TX_ERR_DOMAIN_OK; }
}
#endif

#endif /* __TXERROR_H__ */
