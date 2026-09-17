#ifndef __ABIERROR_H__
#define __ABIERROR_H__

#include <cstdint>
#include <string_view>
#include <txerror.h>

namespace bsv {

/**
 * AbiError_t enumerates the failures of a C ABI boundary to express a call.
 * They say nothing about the transaction: an AbiError_t is always an assertion
 * about the call itself — a length that cannot be represented in the boundary
 * type, or a length that is inconsistent with the pointer it describes.
 *
 * A consumer must therefore classify TX_ERR_DOMAIN_ABI as a local fault of the
 * binding, never as a verdict on the transaction.
 *
 * Values are explicit to guarantee 1:1 correspondence with AbiError_t in the CGO bridge.
 */
enum class AbiError_t : int32_t {
    OK                     = 0,  // unused sentinel
    LengthNegative         = 1,  // a signed length arrived negative
    LengthNotRepresentable = 2,  // the length cannot be expressed in the boundary or platform type
    NullBuffer             = 3,  // positive length with a null pointer
    LengthOverflow         = 4,  // count * sizeof(element) overflows size_t
    Count                  = 5   // sentinel
};

std::string_view AbiErrorString(AbiError_t err);

inline TxError AbiErrorToTxError(AbiError_t e) {
    return bsv::TxErrorAbi(static_cast<int32_t>(e));
}

} // namespace bsv

#endif /* __ABIERROR_H__ */
