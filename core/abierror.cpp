#include <abierror.h>

std::string_view bsv::AbiErrorString(bsv::AbiError_t err) {
    switch (err) {
        case AbiError_t::OK:                     return "ok";
        case AbiError_t::LengthNegative:         return "abi-length-negative";
        case AbiError_t::LengthNotRepresentable: return "abi-length-not-representable";
        case AbiError_t::NullBuffer:             return "abi-null-buffer";
        case AbiError_t::LengthOverflow:         return "abi-length-overflow";
        default:                                 return "unknown-abi-error";
    }
}
