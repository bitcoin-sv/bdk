#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_sha256_backends
#else
#define BOOST_TEST_MODULE test_sha256_backendsd
#endif

#include <boost/test/unit_test.hpp>

#include <openssl/sha.h>

#include "crypto/sha256.h"
#include "crypto/sha256_dispatch.h"

#include "sha256_backend.hpp"

#include "sha256_backend_expectations.h"

namespace
{
    // Installs one backend's transforms for the lifetime of the object. The
    // tests are single-threaded, so swapping the global pointers is safe here.
    class UseBackend
    {
        sha256_dispatch::StreamType saved_stream{sha256_dispatch::stream};
        sha256_dispatch::DBlock2wayType saved_2way{sha256_dispatch::d_block_2way};

    public:
        explicit UseBackend(const bsv::Sha256Backend& backend)
        {
            sha256_dispatch::stream = backend.stream;
            sha256_dispatch::d_block_2way = backend.d_block_2way;
        }

        ~UseBackend()
        {
            sha256_dispatch::stream = saved_stream;
            sha256_dispatch::d_block_2way = saved_2way;
        }

        UseBackend(const UseBackend&) = delete;
        UseBackend& operator=(const UseBackend&) = delete;

        UseBackend(UseBackend&&) = delete;
        UseBackend& operator=(UseBackend&&) = delete;
    };

    std::string Hex(std::span<const uint8_t> bytes)
    {
        std::ostringstream os;
        for(const auto b : bytes)
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        return os.str();
    }

    // Writes msg in pieces of `chunk` bytes so the buffered and the direct
    // transform paths of CSHA256::Write both run
    std::string CSha256Hex(std::span<const uint8_t> msg, size_t chunk)
    {
        CSHA256 hasher;
        for(size_t pos = 0; pos < msg.size(); pos += chunk)
            hasher.Write(msg.data() + pos, std::min(chunk, msg.size() - pos));
        std::array<uint8_t, CSHA256::OUTPUT_SIZE> digest{};
        hasher.Finalize(digest);
        return Hex(digest);
    }

    std::string OpenSslSha256Hex(std::span<const uint8_t> msg)
    {
        std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
        SHA256(msg.data(), msg.size(), digest.data());
        return Hex(digest);
    }

    std::vector<uint8_t> Bytes(const std::string& s) { return {s.begin(), s.end()}; }

    // Bytes 0, 1, 2, ... wrapping at 256
    std::vector<uint8_t> Counting(size_t len)
    {
        std::vector<uint8_t> v(len);
        for(size_t i = 0; i < len; ++i)
            v[i] = static_cast<uint8_t>(i & 0xff);
        return v;
    }

    struct KnownAnswer
    {
        std::string label;
        std::vector<uint8_t> msg;
        std::string digest;
    };

    // Digests computed with `shasum -a 256`, independent of BDK and OpenSSL.
    // The first five are the NIST FIPS 180-4 / NIST CSRC SHA-256 examples.
    std::vector<KnownAnswer> KnownAnswers()
    {
        return {
            {"empty", {}, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
            {"abc", Bytes("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
            {"448-bit", Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
            {"896-bit",
             Bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
                   "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"),
             "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"},
            {"million a", std::vector<uint8_t>(1000000, 'a'),
             "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
            {"counting 55", Counting(55), "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
            {"counting 56", Counting(56), "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
            {"counting 63", Counting(63), "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
            {"counting 64", Counting(64), "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
            {"counting 65", Counting(65), "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"},
            {"counting 127", Counting(127), "92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976"},
            {"counting 128", Counting(128), "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5"},
            {"counting 129", Counting(129), "5099c6a56203f9687f7d33f4bfdf576d31dc91f6b695ecea38b2770c87631135"},
            {"counting 1000", Counting(1000), "a8af099bf2e878609558dbf69d8f88f4a31040a8cf84b549a0cfa912f12ffc3f"},
        };
    }

    uint64_t TestSeed()
    {
        const char* v = std::getenv("BDK_SHA256_TEST_SEED");
        return v != nullptr && *v != '\0' ? std::strtoull(v, nullptr, 0) : 20261016ull;
    }

    std::array<uint8_t, 32> OpenSslSha256d(std::span<const uint8_t> msg)
    {
        std::array<uint8_t, 32> once{};
        std::array<uint8_t, 32> twice{};
        SHA256(msg.data(), msg.size(), once.data());
        SHA256(once.data(), once.size(), twice.data());
        return twice;
    }
}

BOOST_AUTO_TEST_SUITE(test_sha256_backends)

BOOST_AUTO_TEST_CASE(selects_the_best_backend_the_cpu_supports)
{
    const std::string& active = bsv::SelectSha256Backend();
    std::cout << "SHA-256 backend: " << active << '\n';

    BOOST_TEST(active == sha256_test::ExpectedSelection());
    const auto& supported = bsv::Sha256SupportedBackends();
    BOOST_REQUIRE(!supported.empty());
    BOOST_TEST(std::string{supported.front().name} == "standard");
    sha256_test::CheckInstalledBackend();
}

BOOST_AUTO_TEST_CASE(selection_happens_once)
{
    const std::string* first = &bsv::SelectSha256Backend();
    const std::string* second = &bsv::SelectSha256Backend();
    BOOST_TEST(first == second);
}

BOOST_AUTO_TEST_CASE(known_answers_through_every_backend, *boost::unit_test::timeout(120))
{
    for(const auto& backend : bsv::Sha256SupportedBackends())
    {
        std::cout << "known answers through " << backend.name << '\n';
        const UseBackend use{backend};
        for(const auto& ka : KnownAnswers())
        {
            for(const size_t chunk : {size_t{1}, size_t{63}, size_t{64}, size_t{65}, size_t{1} << 20})
            {
                BOOST_TEST_CONTEXT(backend.name << " " << ka.label << " in pieces of " << chunk)
                {
                    BOOST_TEST(CSha256Hex(ka.msg, chunk) == ka.digest);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(double_sha256_of_64_byte_pairs)
{
    // Bitcoin Core's TransformD64 vector: two 64-byte pairs, double-SHA-256 of each
    const std::string text =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Et m";
    std::array<uint8_t, 128> pairs{};
    std::memcpy(pairs.data(), text.data(), pairs.size());
    const std::string expected =
        "093ac4d00ff757e172857942fee7e0a0fc52d7db076345fb53147d172286f052"
        "48b6119e6e48816dcc571fb297a8d5259b82aa89e2fd2d56e828830be2fa53b7";

    std::mt19937_64 rng{TestSeed()};
    std::vector<std::array<uint8_t, 128>> random_pairs(64);
    for(auto& p : random_pairs)
        std::generate(p.begin(), p.end(), [&] { return static_cast<uint8_t>(rng()); });

    const auto check_1way = [&](sha256_dispatch::DBlock1wayType d1, const std::string& name) {
        std::array<uint8_t, 64> out{};
        d1(std::span{pairs}.first<64>(), std::span{out}.first<32>());
        d1(std::span{pairs}.last<64>(), std::span{out}.last<32>());
        BOOST_TEST(Hex(out) == expected, name << " 1way vector");
        for(const auto& p : random_pairs)
        {
            std::array<uint8_t, 32> h{};
            d1(std::span{p}.first<64>(), std::span{h});
            BOOST_TEST(Hex(h) == Hex(OpenSslSha256d(std::span{p}.first<64>())), name << " 1way random");
        }
    };
    check_1way(sha256_dispatch::d_block_1way, "installed");
    check_1way(sha256d64_scalar::Transform, "scalar");

    for(const auto& backend : bsv::Sha256SupportedBackends())
    {
        if(backend.d_block_2way == nullptr)
            continue;
        std::cout << "2-way double SHA-256 through " << backend.name << '\n';
        std::array<uint8_t, 64> out{};
        backend.d_block_2way(pairs, out);
        BOOST_TEST(Hex(out) == expected, backend.name << " 2way vector");
        for(const auto& p : random_pairs)
        {
            backend.d_block_2way(p, out);
            BOOST_TEST(Hex(std::span{out}.first<32>()) == Hex(OpenSslSha256d(std::span{p}.first<64>())),
                       backend.name << " 2way random, first");
            BOOST_TEST(Hex(std::span{out}.last<32>()) == Hex(OpenSslSha256d(std::span{p}.last<64>())),
                       backend.name << " 2way random, second");
        }
    }
}

BOOST_AUTO_TEST_CASE(every_backend_matches_scalar_on_random_messages, *boost::unit_test::timeout(120))
{
    const uint64_t seed = TestSeed();
    std::cout << "differential seed (BDK_SHA256_TEST_SEED): " << seed << '\n';
    std::mt19937_64 rng{seed};
    std::uniform_int_distribution<size_t> length{0, 4200};

    const auto& backends = bsv::Sha256SupportedBackends();
    for(int i = 0; i < 2000; ++i)
    {
        // Each message starts at offset 0..15 into its buffer, so the transforms
        // also see unaligned multi-block input, as they do in production
        const size_t offset = static_cast<size_t>(i % 16);
        std::vector<uint8_t> storage(offset + length(rng));
        std::generate(storage.begin(), storage.end(), [&] { return static_cast<uint8_t>(rng()); });
        const std::span<const uint8_t> msg = std::span<const uint8_t>{storage}.subspan(offset);
        const size_t chunk = i % 3 == 0 ? msg.size() + 1 // one Write: the whole message goes to the transform at once
                                        : std::uniform_int_distribution<size_t>{1, msg.size() + 1}(rng);

        std::string scalar;
        {
            const UseBackend use{backends.front()};
            scalar = CSha256Hex(msg, chunk);
        }
        BOOST_TEST(scalar == OpenSslSha256Hex(msg),
                   "scalar vs OpenSSL, message " << i << ", " << msg.size() << " bytes at offset " << offset);
        for(const auto& backend : backends)
        {
            const UseBackend use{backend};
            BOOST_TEST(CSha256Hex(msg, chunk) == scalar,
                       backend.name << " vs scalar, message " << i << ", " << msg.size() << " bytes at offset "
                                    << offset << ", pieces of " << chunk);
        }
    }
}

// The transforms called directly, as CSHA256 and the merkle code do, on input
// and output that start 1 to 15 bytes past an aligned address
BOOST_AUTO_TEST_CASE(direct_transforms_at_unaligned_offsets, *boost::unit_test::timeout(120))
{
    std::mt19937_64 rng{TestSeed()};
    std::vector<uint8_t> buffer(16 + 8 * 64);
    std::generate(buffer.begin(), buffer.end(), [&] { return static_cast<uint8_t>(rng()); });
    const std::array<uint32_t, 8> init = {0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
                                          0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul};
    const auto& backends = bsv::Sha256SupportedBackends();

    for(size_t offset = 1; offset < 16; ++offset)
    {
        for(const size_t blocks : {size_t{1}, size_t{2}, size_t{3}, size_t{4}, size_t{8}})
        {
            const std::span<const uint8_t> input{buffer.data() + offset, blocks * 64};
            std::array<uint32_t, 8> scalar = init;
            sha256_stream_scalar::Transform(input, scalar);
            for(const auto& backend : backends)
            {
                std::array<uint32_t, 8> state = init;
                backend.stream(input, state);
                BOOST_TEST((state == scalar), backend.name << " stream, " << blocks << " blocks at offset " << offset);
            }
        }

        const std::span<const uint8_t, 128> pairs{buffer.data() + offset, 128};
        const auto first = OpenSslSha256d(pairs.first<64>());
        const auto second = OpenSslSha256d(pairs.last<64>());
        for(const auto& backend : backends)
        {
            if(backend.d_block_2way == nullptr)
                continue;
            std::array<uint8_t, 16 + 64> out_storage{};
            const std::span<uint8_t, 64> out{out_storage.data() + offset, 64};
            backend.d_block_2way(pairs, out);
            BOOST_TEST(Hex(out.first<32>()) == Hex(first), backend.name << " 2way first, offset " << offset);
            BOOST_TEST(Hex(out.last<32>()) == Hex(second), backend.name << " 2way second, offset " << offset);
        }
    }
}

// Not a pass/fail check: prints raw transform throughput per backend into the
// test log, so CI records it for each runner's CPU
BOOST_AUTO_TEST_CASE(throughput_per_backend, *boost::unit_test::timeout(120))
{
    constexpr size_t buffer_bytes = size_t{16} << 20;
    constexpr int rounds = 4;
    std::vector<uint8_t> buffer(buffer_bytes, 0x5a);
    std::array<uint8_t, 128> pairs{};
    std::array<uint8_t, 64> out{};

    std::cout << "SHA-256 throughput, " << (rounds * buffer_bytes >> 20) << " MiB per backend:" << '\n';
    for(const auto& backend : bsv::Sha256SupportedBackends())
    {
        std::array<uint32_t, 8> state{};
        const auto start = std::chrono::steady_clock::now();
        for(int r = 0; r < rounds; ++r)
            backend.stream(buffer, state);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "  " << std::left << std::setw(10) << backend.name << " stream "
                  << std::right << std::fixed << std::setprecision(0) << std::setw(7)
                  << (rounds * buffer_bytes / 1e6) / secs << " MB/s";

        if(backend.d_block_2way != nullptr)
        {
            constexpr int calls = 1 << 20;
            const auto start2 = std::chrono::steady_clock::now();
            for(int c = 0; c < calls; ++c)
            {
                pairs[0] = static_cast<uint8_t>(c);
                backend.d_block_2way(pairs, out);
            }
            const double secs2 = std::chrono::duration<double>(std::chrono::steady_clock::now() - start2).count();
            std::cout << ", 2way " << std::setw(6) << (2.0 * calls / 1e6) / secs2 << " M pair-hashes/s";
        }
        std::cout << '\n';
        BOOST_TEST((state != std::array<uint32_t, 8>{}));
    }
}

BOOST_AUTO_TEST_SUITE_END()
