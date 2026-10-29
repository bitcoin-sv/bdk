#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
#include <string>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_sha256_select_fallback
#else
#define BOOST_TEST_MODULE test_sha256_select_fallbackd
#endif

#include <boost/test/unit_test.hpp>

#include "crypto/sha256.h"
#include "crypto/sha256_dispatch.h"

#include "sha256_backend.hpp"
#include "txvalidator.hpp"

// BDK selects the SHA-256 backend while the program is loaded
// (core/sha256_hasher.cpp). If that selection throws, BDK must keep the scalar
// transforms for good: no later call may select again, since that would write
// the sha256_dispatch pointers while other threads can be hashing.
//
// This executable builds its own copy of core/sha256_backend.cpp with
// BDK_SHA256_SELECTION_TEST_HOOK, which calls AfterTransformsInstalled() below
// once the fastest transforms are installed. The first call throws, as a failed
// allocation of the result would, and FallbackKept() below runs once BDK has
// put the scalar transforms back. The allocation after that, of the "standard"
// result, fails as well. Everything else, including the load-time
// initializer, comes from bdk_core. This must stay the only test case in its
// executable, in a process of its own.

namespace sha256_test
{
    // Zero-initialized, so all are already set when BDK's load-time selection
    // runs. Written only then, while the program is loaded on one thread.
    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
    int selection_attempts{0};
    int fallbacks_kept{0};
    // From the failed selection's throw until the scalar transforms are kept
    bool building_fallback{false};
    int fallback_allocations{0};
    // Then the next allocation, of the "standard" result, fails once
    bool fail_next_allocation{false};
    int failed_result_allocations{0};
    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

    void AfterTransformsInstalled()
    {
        if(++selection_attempts == 1)
        {
            building_fallback = true;
            throw std::bad_alloc{};
        }
    }

    void FallbackKept()
    {
        building_fallback = false;
        fail_next_allocation = true;
        ++fallbacks_kept;
    }
}

// This executable's operator new and delete, to count the allocations made
// while BDK keeps the scalar transforms, which happens when memory has run out
// and so must not allocate, and to fail the allocation after it. The nothrow
// and array forms reach these by default.
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
void* operator new(std::size_t size)
{
    if(sha256_test::building_fallback)
        ++sha256_test::fallback_allocations;
    if(sha256_test::fail_next_allocation)
    {
        sha256_test::fail_next_allocation = false;
        ++sha256_test::failed_result_allocations;
        throw std::bad_alloc{};
    }
    if(void* p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void* p) noexcept { std::free(p); }

void operator delete[](void* p) noexcept { std::free(p); }

void operator delete(void* p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete[](void* p, std::size_t /*size*/) noexcept { std::free(p); }

// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

namespace
{
    // The scalar transform under another address, to see whether anything
    // writes sha256_dispatch::stream after load
    void ScalarSentinel(std::span<const std::uint8_t> ip, std::span<std::uint32_t, 8> op)
    {
        sha256_stream_scalar::Transform(ip, op);
    }

    // FIPS 180-4 example: SHA-256("abc")
    constexpr std::array<std::uint8_t, CSHA256::OUTPUT_SIZE>
        abc_digest{0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                   0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                   0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
}

BOOST_AUTO_TEST_SUITE(test_sha256_select_fallback)

BOOST_AUTO_TEST_CASE(failed_selection_at_load_keeps_scalar)
{
    // The load-time selection ran once, installed the fastest transforms and
    // threw, and BDK kept the scalar transforms without allocating. Building
    // the "standard" result after that failed too, so the load-time call threw
    BOOST_TEST(sha256_test::selection_attempts == 1);
    BOOST_TEST(sha256_test::fallbacks_kept == 1);
    BOOST_TEST(sha256_test::fallback_allocations == 0);
    BOOST_TEST(sha256_test::failed_result_allocations == 1);

    // And put the scalar transforms back, which only shows where faster ones
    // were installed
    if(bsv::Sha256SupportedBackends().size() > 1)
    {
        BOOST_TEST(sha256_dispatch::stream == sha256_stream_scalar::Transform);
        BOOST_TEST(sha256_dispatch::d_block_2way == nullptr);
    }
    else
    {
        std::cout
            << "No hardware SHA-256 transform on this CPU: scalar restore not checked"
            << '\n';
    }

    // Nothing that selects on a binding's behalf writes the pointers again,
    // including this first call, which builds the result the load-time call
    // could not
    sha256_dispatch::stream = ScalarSentinel;
    const std::string* first = &bsv::SelectSha256Backend();
    BOOST_TEST(*first == "standard");
    {
        const bsv::CTxValidator by_network{bsv::TxValidationNetwork::Main};
        const bsv::CTxValidator by_chain_name{"main"};
    }
    BOOST_TEST(&bsv::SelectSha256Backend() == first);
    BOOST_TEST(bsv::SelectSha256Backend() == "standard");
    BOOST_TEST(sha256_test::selection_attempts == 1);
    BOOST_TEST(sha256_dispatch::stream == ScalarSentinel);
    BOOST_TEST(sha256_dispatch::d_block_2way == nullptr);

    // And hashing still gives the right answer
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    std::array<std::uint8_t, CSHA256::OUTPUT_SIZE> digest{};
    CSHA256().Write(abc.data(), abc.size()).Finalize(digest);
    BOOST_TEST(digest == abc_digest);

    sha256_dispatch::stream = sha256_stream_scalar::Transform;
}

BOOST_AUTO_TEST_SUITE_END()
