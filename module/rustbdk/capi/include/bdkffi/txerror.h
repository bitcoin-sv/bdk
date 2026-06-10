#ifndef BDKFFI_TXERROR_H
#define BDKFFI_TXERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bdkffi_tx_error_domain {
    BDKFFI_TX_ERR_DOMAIN_OK = 0,
    BDKFFI_TX_ERR_DOMAIN_SCRIPT = 1,
    BDKFFI_TX_ERR_DOMAIN_DOS = 2,
    BDKFFI_TX_ERR_DOMAIN_EXCEPTION = 3
} bdkffi_tx_error_domain;

typedef struct bdkffi_TxError {
    int32_t domain;
    int32_t code;
} bdkffi_TxError;

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_TXERROR_H */
