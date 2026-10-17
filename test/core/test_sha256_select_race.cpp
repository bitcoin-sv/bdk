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

// The first SHA-256 backend selection in this process happens while several
// threads enter BDK at once through the paths the Go and Rust bindings use:
// constructing a validator (both constructors), asking for the backend, and the
// asm conversions, which neither select nor hash. Each thread that hashes does
// so only after its own entry point, so its reads of the sha256_dispatch
// pointers are ordered after the one write. Run under ThreadSanitizer to check
// that ordering; without it the test still checks every digest and the final
// selection.
//
// This must stay the only test case in its executable: nothing may select
// before the threads start.
//
// Positive control for the ThreadSanitizer run: with
// BDK_TEST_SHA256_RACE_CONTROL=1 the asm threads also hash straight after the
// start line, without entering BDK through a selecting path, and ThreadSanitizer
// must report the race on sha256_dispatch::stream.

namespace
{
    bool HashMatchesOpenSsl(const std::vector<uint8_t>& msg)
    {
        std::array<uint8_t, CSHA256::OUTPUT_SIZE> ours{};
        CSHA256().Write(msg.data(), msg.size()).Finalize(ours);
        std::array<uint8_t, SHA256_DIGEST_LENGTH> theirs{};
        SHA256(msg.data(), msg.size(), theirs.data());
        return ours == theirs;
    }
}

BOOST_AUTO_TEST_SUITE(test_sha256_select_race)

BOOST_AUTO_TEST_CASE(first_selection_while_threads_enter_bdk, *boost::unit_test::timeout(120))
{
    const char* control = std::getenv("BDK_TEST_SHA256_RACE_CONTROL");
    const bool race_on_purpose = control != nullptr && std::string{control} == "1";

    BOOST_REQUIRE(sha256_dispatch::stream == sha256_stream_scalar::Transform);

    constexpr int per_role = 4;
    enum Role { NetworkValidator, ChainNameValidator, Getter, Asm, RoleCount };
    constexpr int threads = per_role * RoleCount;

    std::latch go{threads};
    std::vector<int> ok(threads, 0);
    std::vector<std::thread> pool;
    for(int i = 0; i < threads; ++i)
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
            case Asm:
            {
                // The positive control hashes without selecting first
                bool same = race_on_purpose ? HashMatchesOpenSsl(msg) : true;
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
    for(auto& t : pool)
        t.join();

    for(int i = 0; i < threads; ++i)
        BOOST_TEST(ok[i] == 1, "thread " << i << " role " << i % RoleCount);

    const char* forced = std::getenv("BDK_SHA256_FORCE_SCALAR");
    const bool is_forced = forced != nullptr && *forced != '\0' && std::string{forced} != "0";
    const auto& expected = is_forced ? bsv::Sha256SupportedBackends().front() : bsv::Sha256SupportedBackends().back();
    BOOST_TEST(sha256_dispatch::stream == expected.stream);
    BOOST_TEST(sha256_dispatch::d_block_2way == expected.d_block_2way);
    std::cout << "SHA-256 backend: " << bsv::SelectSha256Backend() << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
