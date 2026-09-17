#ifndef __ABIGUARD_HPP__
#define __ABIGUARD_HPP__

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>

#include <core/abierror.h>

/**
 * Internal helpers shared by the CGO shim sources. Not a public header: it is
 * neither installed nor reachable from the generated cgo preamble.
 *
 * Every buffer argument crossing the C ABI is turned into a span here, so that a
 * length that cannot be expressed is rejected at one place rather than trusted at
 * each call site. Rejections are reported by throwing AbiArgumentError, which each
 * extern "C" entry point catches and converts to its own failure channel.
 */
namespace bdkcgo {

class AbiArgumentError : public std::exception {
public:
    explicit AbiArgumentError(bsv::AbiError_t code) : mCode(code) {}

    bsv::AbiError_t code() const noexcept { return mCode; }

    // AbiErrorString returns a view over a string literal, so it is null terminated.
    const char* what() const noexcept override { return bsv::AbiErrorString(mCode).data(); }

private:
    bsv::AbiError_t mCode;
};

/**
 * abiToSize narrows an ABI length to size_t. On a 64-bit target the check is a
 * no-op; it exists so this boundary is correct by construction rather than by
 * platform assumption — the headers ship next to per-platform archives.
 */
inline size_t abiToSize(uint64_t len)
{
    if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
        if (len > static_cast<uint64_t>(SIZE_MAX)) {
            throw AbiArgumentError(bsv::AbiError_t::LengthNotRepresentable);
        }
    }
    return static_cast<size_t>(len);
}

/**
 * abiToCount is abiToSize plus the check that count * sizeof(T) — the byte size of
 * the span or the allocation the count is about to size — does not overflow size_t.
 */
template <typename T>
inline size_t abiToCount(uint64_t count)
{
    const size_t n = abiToSize(count);
    if (n > SIZE_MAX / sizeof(T)) {
        throw AbiArgumentError(bsv::AbiError_t::LengthOverflow);
    }
    return n;
}

inline std::span<const uint8_t> byte_span(const char* ptr, uint64_t len)
{
    const size_t n = abiToCount<uint8_t>(len);
    if (n == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw AbiArgumentError(bsv::AbiError_t::NullBuffer);
    }
    return { static_cast<const uint8_t*>(reinterpret_cast<const void*>(ptr)), n };
}

inline std::span<const int32_t> int32_span(const int32_t* ptr, uint64_t len)
{
    const size_t n = abiToCount<int32_t>(len);
    if (n == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw AbiArgumentError(bsv::AbiError_t::NullBuffer);
    }
    return { ptr, n };
}

inline std::span<const uint32_t> uint32_span(const uint32_t* ptr, uint64_t len)
{
    const size_t n = abiToCount<uint32_t>(len);
    if (n == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw AbiArgumentError(bsv::AbiError_t::NullBuffer);
    }
    return { ptr, n };
}

} // namespace bdkcgo

#endif /* __ABIGUARD_HPP__ */
