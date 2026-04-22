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
        default:                                return "unknown-dos-error";
    }
}
