#include <doserror.hpp>

std::string_view bsv::DoSErrorString(bsv::DoSError_t err) {
    switch (err) {
        case DoSError_t::OK:                    return "ok";
        case DoSError_t::NullPrevout:           return "bad-txns-prevout-null";
        case DoSError_t::P2SHOutputPostGenesis: return "bad-txns-vout-p2sh";
        case DoSError_t::SigopsConsensus:       return "bad-txn-sigops";
        case DoSError_t::SigopsPolicy:          return "bad-txns-too-many-sigops";
        case DoSError_t::NotFreeConsolidation:  return "not-free-consolidation";
        case DoSError_t::NotStandard:           return "not-standard";
        case DoSError_t::VinEmpty:              return "bad-txns-vin-empty";
        case DoSError_t::VoutEmpty:             return "bad-txns-vout-empty";
        case DoSError_t::Oversize:              return "bad-txns-oversize";
        case DoSError_t::OutputNegative:        return "bad-txns-vout-negative";
        case DoSError_t::OutputTooLarge:        return "bad-txns-vout-toolarge";
        case DoSError_t::OutputTotalTooLarge:   return "bad-txns-txouttotal-toolarge";
        case DoSError_t::CoinbaseNotAllowed:    return "bad-tx-coinbase";
        case DoSError_t::DuplicateInputs:       return "bad-txns-inputs-duplicate";
        default:                                return "unknown-dos-error";
    }
}
