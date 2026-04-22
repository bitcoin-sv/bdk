#ifndef __TXERROR_CGO_H__
#define __TXERROR_CGO_H__

#ifdef __cplusplus
/* In C++ bridge compilation, use the authoritative core header to avoid duplicate
   type definitions when both txvalidator_cgo.h and core headers appear in the same TU. */
#include <txerror.h>
#else
/* In C/CGO compilation, core/txerror.h is not on the include path.
   Define TxError and TxErrorDomain_t directly — must stay in sync with core/txerror.h. */
#include <stdint.h>

typedef enum {
    TX_ERR_DOMAIN_OK        = 0,
    TX_ERR_DOMAIN_SCRIPT    = 1,
    TX_ERR_DOMAIN_DOS       = 2,
    TX_ERR_DOMAIN_EXCEPTION = 3
} TxErrorDomain_t;

typedef struct {
    int32_t domain;
    int32_t code;
} TxError;

#endif /* __cplusplus */

#endif /* __TXERROR_CGO_H__ */
