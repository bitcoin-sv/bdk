#ifndef __DOSERROR_HPP__
#define __DOSERROR_HPP__

#include <cstdint>
#include <string_view>
#include <txerror.h>

namespace bsv {

/**
 * DoSError_t enumerates transaction-level validation failures produced by CTxValidator.
 * These correspond to the DoS-flagged reject reasons in bitcoin-sv's CValidationState
 * (state.DoS(100,...) for consensus, state.DoS(0,...) for policy), expressed as a
 * proper typed enum rather than unstructured strings.
 *
 * Values are explicit to guarantee 1:1 correspondence with DoSError_t in the CGO bridge.
 */
enum class DoSError_t : int32_t {
    OK                    = 0,
    // Consensus checks — both peer and block context (DoS=100 in bitcoin-sv)
    NullPrevout           = 1,  // "bad-txns-prevout-null"
    P2SHOutputPostGenesis = 2,  // "bad-txns-vout-p2sh"
    SigopsConsensus       = 3,  // "bad-txn-sigops" (pre-Genesis, without P2SH)
    // Policy checks — peer/mempool context only (DoS=0 in bitcoin-sv)
    SigopsPolicy          = 4,  // "bad-txns-too-many-sigops" (with P2SH redeem scripts)
    NotFreeConsolidation  = 5,  // underfunded tx, does not qualify as free consolidation
    NotStandard           = 6,  // tx or input fails standardness check (REJECT_NONSTANDARD)
    // CheckTransactionCommon checks — both peer and block context
    VinEmpty              = 7,  // "bad-txns-vin-empty"       (DoS 10)
    VoutEmpty             = 8,  // "bad-txns-vout-empty"      (DoS 10)
    Oversize              = 9,  // "bad-txns-oversize"        (DoS 100)
    OutputNegative        = 10, // "bad-txns-vout-negative"   (DoS 100)
    OutputTooLarge        = 11, // "bad-txns-vout-toolarge"   (DoS 100)
    OutputTotalTooLarge   = 12, // "bad-txns-txouttotal-toolarge" (DoS 100)
    CoinbaseNotAllowed    = 13, // "bad-tx-coinbase" (DoS 100) — coinbase in non-coinbase context
    Count                 = 14  // sentinel
};

std::string_view DoSErrorString(DoSError_t err);

inline TxError DoSErrorToTxError(DoSError_t e) {
    return bsv::TxErrorDoS(static_cast<int32_t>(e));
}

} // namespace bsv

#endif /* __DOSERROR_HPP__ */
