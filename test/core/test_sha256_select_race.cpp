#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <latch>
#include <string>
#include <thread>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_sha256_select_race
#else
#define BOOST_TEST_MODULE test_sha256_select_raced
#endif

#include <boost/test/unit_test.hpp>

#include <openssl/sha.h>

#include "crypto/sha256.h"
#include "crypto/sha256_dispatch.h"
#include "script/script.h"

#include "assembler.h"
#include "sha256_backend.hpp"
#include "txvalidator.hpp"

#include "sha256_backend_expectations.h"

// BDK selects the SHA-256 backend while the program is loaded
// (core/sha256_hasher.cpp), so it is installed before this test does anything,
// and hashing on many threads at once never races with the one write to the
// sha256_dispatch pointers. The threads enter BDK through the paths the Go and
// Rust bindings use: constructing a validator (both constructors), asking for
// the backend, and the asm conversions, plus threads that hash straight away
// without entering BDK at all. Run under ThreadSanitizer to check the ordering;
// without it the test still checks every digest and the installed backend.
//
// This must stay the only test case in its executable, in a process of its own,
// so that nothing in the test can have made the selection.
//
// Positive control for the ThreadSanitizer run: with
// BDK_TEST_SHA256_RACE_CONTROL=1 one more thread writes sha256_dispatch::stream
// (with the value it already has) while the others hash, and ThreadSanitizer
// must report the race on sha256_dispatch::stream.

namespace
{
    // Several Write calls, so each thread reads the dispatch pointer repeatedly
    bool HashMatchesOpenSsl(const std::vector<uint8_t>& msg)
    {
        CSHA256 hasher;
        constexpr size_t piece = 8 * 1024;
        for(size_t pos = 0; pos < msg.size(); pos += piece)
            hasher.Write(msg.data() + pos, std::min(piece, msg.size() - pos));
        std::array<uint8_t, CSHA256::OUTPUT_SIZE> ours{};
        hasher.Finalize(ours);
        std::array<uint8_t, SHA256_DIGEST_LENGTH> theirs{};
        SHA256(msg.data(), msg.size(), theirs.data());
        return ours == theirs;
    }
}

BOOST_AUTO_TEST_SUITE(test_sha256_select_race)

BOOST_AUTO_TEST_CASE(selected_at_load_before_threads_hash, *boost::unit_test::timeout(120))
{
    const char* control = std::getenv("BDK_TEST_SHA256_RACE_CONTROL");
    const bool race_on_purpose = control != nullptr && std::string{control} == "1";

    // Before this process has constructed a validator, asked for the backend
    // or hashed anything, the backend is already installed
    sha256_test::CheckInstalledBackend();

    constexpr int per_role = 4;
    enum Role { NetworkValidator, ChainNameValidator, Getter, HashOnly, Asm, RoleCount };
    constexpr int hashers = per_role * RoleCount;
    const int threads = hashers + (race_on_purpose ? 1 : 0);

    std::latch go{threads};
    std::vector<int> ok(hashers, 0);
    std::vector<std::thread> pool;
    for(int i = 0; i < hashers; ++i)
    {
        pool.emplace_back([&, i] {
            std::vector<uint8_t> msg(64 * 1024 + i, static_cast<uint8_t>(i));
            go.arrive_and_wait();
            switch(static_cast<Role>(i % RoleCount))
            {
            case NetworkValidator:
            {
                const bsv::CTxValidator validator{bsv::TxValidationNetwork::Main};
                ok[i] = HashMatchesOpenSsl(msg);
                break;
            }
            case ChainNameValidator:
            {
                const bsv::CTxValidator validator{std::string{"main"}};
                ok[i] = HashMatchesOpenSsl(msg);
                break;
            }
            case Getter:
                ok[i] = !bsv::SelectSha256Backend().empty() && HashMatchesOpenSsl(msg);
                break;
            case HashOnly:
                ok[i] = HashMatchesOpenSsl(msg);
                break;
            case Asm:
            {
                bool same = true;
                for(int n = 0; n < 200; ++n)
                {
                    const CScript script = bsv::from_asm("DUP HASH160 EQUALVERIFY CHECKSIG");
                    same = same && bsv::to_asm(std::span<const uint8_t>{script.data(), script.size()})
                                       == "DUP HASH160 EQUALVERIFY CHECKSIG";
                }
                ok[i] = same;
                break;
            }
            case RoleCount:
                break;
            }
        });
    }
    if(race_on_purpose)
    {
        pool.emplace_back([&] {
            go.arrive_and_wait();
            // An unsynchronised write while the others hash: the race the
            // load-time selection prevents. Volatile, so the compiler cannot
            // drop a store of the value just read.
            volatile sha256_dispatch::StreamType& stream = sha256_dispatch::stream;
            stream = sha256_dispatch::StreamType{stream};
        });
    }
    for(auto& t : pool)
        t.join();

    for(int i = 0; i < hashers; ++i)
        BOOST_TEST(ok[i] == 1, "thread " << i << " role " << i % RoleCount);

    sha256_test::CheckInstalledBackend();
    const std::string expected = sha256_test::ExpectedSelection();
    BOOST_TEST(bsv::SelectSha256Backend() == expected);
    std::cout << "SHA-256 backend: " << bsv::SelectSha256Backend() << std::endl;

    // BDK_SHA256_FORCE_SCALAR was read at load: changing it now changes nothing
    const bool forced_at_start = sha256_test::ForcedScalar();
#if defined(_WIN32)
    ::_putenv_s("BDK_SHA256_FORCE_SCALAR", forced_at_start ? "0" : "1");
#else
    ::setenv("BDK_SHA256_FORCE_SCALAR", forced_at_start ? "0" : "1", 1);
#endif
    BOOST_TEST(bsv::SelectSha256Backend() == expected);
    sha256_test::CheckInstalledBackend(forced_at_start);
}

BOOST_AUTO_TEST_SUITE_END()
