#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif
#if defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_txvalidator
#else
#define BOOST_TEST_MODULE test_txvalidatord
#endif

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>

#include "streams.h"
#include "version.h"
#include "key.h"
#include "pubkey.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "script/script_flags.h"
#include "script/sighashtype.h"
#include "utilstrencodings.h"
#include "verify_script_flags.h"
#include "consensus/params.h"
#include "protocol_era.h"
#include "chainparams.h"
#include "chainparamsbase.h"

#include "txvalidator.hpp"
#include "internal/txvalidator_internal.hpp"
#include "chainparams_bdk.hpp"
#include "extendedTx.hpp"
#include "txerror.h"

namespace ba = boost::algorithm;

using namespace std;
using namespace bsv;

// `test_txvalidator -- --parallel-input-threshold=N` runs every test with the
// parallel input threshold set to N; ctest runs the suite once with 2, so every
// multi-input tx is verified on several threads. `--input-pool-cores=N` sizes
// the input pool to at most N cores, to reproduce a CI-sized pool locally.
// The parallel input threshold the library starts with, read before any
// test or argument changes it.
static size_t shippedParallelInputThreshold = 0;
// The helper limit the library starts with, read the same way.
static int64_t shippedParallelInputHelpers = 0;
// Whether the test-only input counters were on when the library started.
static bool shippedInputCountersEnabled = true;

struct ParallelInputThresholdFromArgs {
    ParallelInputThresholdFromArgs()
    {
        shippedParallelInputThreshold = bsv::detail::SetParallelInputThresholdForTesting(0);
        shippedParallelInputHelpers = bsv::GetParallelInputHelpers();
        bsv::detail::SetParallelInputThresholdForTesting(shippedParallelInputThreshold);
        // The pool tests read the test-only input counters; they are off by default.
        shippedInputCountersEnabled = bsv::detail::SetInputCountersEnabledForTesting(true);
        const std::string threshold = "--parallel-input-threshold=";
        const std::string cores = "--input-pool-cores=";
        const auto& suite = boost::unit_test::framework::master_test_suite();
        for (int i = 1; i < suite.argc; ++i) {
            const std::string arg = suite.argv[i];
            if (arg.rfind(threshold, 0) == 0)
                bsv::detail::SetParallelInputThresholdForTesting(std::stoull(arg.substr(threshold.size())));
            if (arg.rfind(cores, 0) == 0)
                bsv::detail::SetInputPoolCoresForTesting(std::stoull(arg.substr(cores.size())));
        }
    }
};
BOOST_TEST_GLOBAL_FIXTURE(ParallelInputThresholdFromArgs);

// Ends the process (status 124) when one test case runs longer than
// CASE_LIMIT, naming the case. A hung parallel job (a caller waiting for helpers that never
// leave) would otherwise block the binary until ctest's TIMEOUT, and the
// parallel suite's Boost timeouts don't cover the outer suite's cases. No
// test case takes more than a few seconds normally, a minute under TSAN.
struct HangWatchdog : boost::unit_test::test_observer {
    static constexpr std::chrono::seconds CASE_LIMIT{ 120 };

    HangWatchdog()
    {
        boost::unit_test::framework::register_observer(*this);
        watcher = std::thread([this] {
            std::unique_lock lock{ mutex };
            while (!stop) {
                woken.wait_for(lock, std::chrono::seconds(1));
                if (running && std::chrono::steady_clock::now() - started > CASE_LIMIT) {
                    // _Exit, not abort: Boost.Test catches SIGABRT.
                    std::fprintf(stderr, "test case %s ran longer than %lld s: exiting with status 124\n",
                        name.c_str(), static_cast<long long>(CASE_LIMIT.count()));
                    std::fflush(nullptr);
                    std::_Exit(124);
                }
            }
        });
    }
    ~HangWatchdog()
    {
        {
            std::lock_guard lock{ mutex };
            stop = true;
        }
        woken.notify_one();
        watcher.join();
        boost::unit_test::framework::deregister_observer(*this);
    }

    void test_unit_start(const boost::unit_test::test_unit& unit) override
    {
        if (unit.p_type != boost::unit_test::TUT_CASE)
            return;
        std::lock_guard lock{ mutex };
        running = true;
        started = std::chrono::steady_clock::now();
        name = unit.full_name();
    }
    void test_unit_finish(const boost::unit_test::test_unit& unit, unsigned long) override
    {
        if (unit.p_type != boost::unit_test::TUT_CASE)
            return;
        std::lock_guard lock{ mutex };
        running = false;
    }

    std::mutex mutex;
    std::condition_variable woken;
    bool stop{ false };
    bool running{ false };
    std::chrono::steady_clock::time_point started;
    std::string name;
    std::thread watcher;
};
BOOST_TEST_GLOBAL_FIXTURE(HangWatchdog);

BOOST_AUTO_TEST_SUITE(test_txvalidator)

// A custom chain params
class MyParams : public CChainParams {
public:
    static const std::string CustomName;

    MyParams() {
    }
};
const std::string MyParams::CustomName = "MyParams";

// Registration custom chain params
static bsv::RegisterCustomChainParams<MyParams> myParamsReg(MyParams::CustomName);

BOOST_AUTO_TEST_CASE(custom_chainparams)
{
    BOOST_TEST(bsv::CreateCustomChainParams(CBaseChainParams::MAIN) != nullptr);
    BOOST_TEST(bsv::CreateCustomChainParams(CBaseChainParams::TESTNET) != nullptr);
    BOOST_TEST(bsv::CreateCustomChainParams(CBaseChainParams::REGTEST) != nullptr);
    BOOST_TEST(bsv::CreateCustomChainParams(CBaseChainParams::STN) != nullptr);

    BOOST_TEST(bsv::CreateCustomChainParams(CustomChainParams::TERATESTNET) != nullptr);
    BOOST_TEST(bsv::CreateCustomChainParams(CustomChainParams::TERASCALINGTESTNET) != nullptr);

    BOOST_TEST(bsv::CreateCustomChainParams(MyParams::CustomName) != nullptr);

    // Unknown network should throw a exception
    BOOST_CHECK_THROW(bsv::CreateCustomChainParams(std::string{ "unknownchain" }), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(built_in_network_parameters_match_chainparams)
{
    struct NetworkCase {
        bsv::TxValidationNetwork network;
        const char* chainName;
    };
    const std::array networks{
        NetworkCase{bsv::TxValidationNetwork::Main, "main"},
        NetworkCase{bsv::TxValidationNetwork::Test, "test"},
        NetworkCase{bsv::TxValidationNetwork::Stn, "stn"},
        NetworkCase{bsv::TxValidationNetwork::Regtest, "regtest"},
        NetworkCase{bsv::TxValidationNetwork::TeraTestnet, "teratestnet"},
        NetworkCase{bsv::TxValidationNetwork::TeraScalingTestnet, "tstn"}
    };
    const std::array heights{
        1, 2, 14, 15, 99, 100, 249, 250, 518, 519, 575, 576, 1250,
        1251, 1350, 1351, 2199, 2200, 9999, 10000, 14999, 15000,
        173804, 173805, 330775, 330776, 363724, 363725, 388380,
        388381, 419327, 419328, 478557, 478558, 504030, 504031,
        581884, 581885, 620537, 620538, 770111, 770112, 943815, 943816,
        1155874, 1155875, 1188696, 1188697, 1344301, 1344302,
        1713167, 1713168
    };

    for(const auto& network : networks) {
        const bsv::CTxValidator compact{network.network};
        const bsv::CTxValidator full{std::string{network.chainName}};
        BOOST_CHECK_EQUAL(compact.GetRequireStandard(), full.GetRequireStandard());
        BOOST_CHECK_EQUAL(compact.GetGenesisActivationHeight(), full.GetGenesisActivationHeight());
        BOOST_CHECK_EQUAL(compact.GetChronicleActivationHeight(), full.GetChronicleActivationHeight());
        for(const int32_t height : heights) {
            BOOST_CHECK_EQUAL(
                compact.CalculateFlags(height, height, false),
                full.CalculateFlags(height, height, false));
            BOOST_CHECK_EQUAL(
                compact.CalculateFlags(height, height, true),
                full.CalculateFlags(height, height, true));
        }
    }
}

BOOST_AUTO_TEST_CASE(stn_string_constructor_has_deterministic_activation_flags)
{
    const bsv::CTxValidator compact{bsv::TxValidationNetwork::Stn};

    // CStnParams in SV leaves the BIP65, BIP66 and CSV heights uninitialized.
    // Repeated string construction must use BDK's explicit STN table instead
    // of inheriting platform- and stack-dependent values from those fields.
    for(int iteration = 0; iteration < 32; ++iteration) {
        const bsv::CTxValidator stringNamed{std::string{"stn"}};
        for(const int32_t height : {1, 14, 15, 99, 100, 249, 250, 2199, 2200}) {
            BOOST_CHECK_EQUAL(
                stringNamed.CalculateFlags(height, height, false),
                compact.CalculateFlags(height, height, false));
            BOOST_CHECK_EQUAL(
                stringNamed.CalculateFlags(height, height, true),
                compact.CalculateFlags(height, height, true));
        }
    }
}

BOOST_AUTO_TEST_CASE(get_script_verify_flags)
{
    using namespace std;
    using test_data_type = tuple<ProtocolEra,
        bool,       // require_standard
        bool,       // is_prom_mempool_flags
        uint32_t>;  // expected result
    const vector<test_data_type> test_data{
        {ProtocolEra::Unknown, false, false, 0x1'47df},
        {ProtocolEra::PreGenesis, false, false, 0x1'47df},
        {ProtocolEra::PostGenesis, false, false, 0x5'47df},
        {ProtocolEra::PostChronicle, false, false, 0x15'47df},

        {ProtocolEra::Unknown, true, false, 0x1'47df},
        {ProtocolEra::PreGenesis, true, false, 0x1'47df},
        {ProtocolEra::PostGenesis, true, false, 0x5'47df},
        {ProtocolEra::PostChronicle, true, false, 0x15'47df},

        {ProtocolEra::Unknown, false, true, 0x8001'0000},
        {ProtocolEra::PreGenesis, false, true, 0x8001'0000},
        {ProtocolEra::PostGenesis, false, true, 0x8001'0000},
        {ProtocolEra::PostChronicle, false, true, 0x8001'0000},

        {ProtocolEra::Unknown, true, true, 0x1'47df},
        {ProtocolEra::PreGenesis, true, true, 0x1'47df},
        {ProtocolEra::PostGenesis, true, true, 0x5'47df},
        {ProtocolEra::PostChronicle, true, true, 0x15'47df},
    };
    for (const auto [era,
        require_standard,
        is_prom_mempool_flags,
        expected] : test_data)
    {
        const uint64_t promiscuous_mempool_flags{ 0x8000'0000 };
        const auto flags = GetScriptVerifyFlags(era,
            require_standard,
            is_prom_mempool_flags,
            promiscuous_mempool_flags);
        BOOST_CHECK_EQUAL(expected, flags);
    }
}

BOOST_AUTO_TEST_CASE(get_block_script_flags)
{
    using namespace std;
    using test_data_type = tuple<int32_t,       // block height
        ProtocolEra,
        int32_t>;      // expected result 
    const vector<test_data_type> test_data{
        {  0, ProtocolEra::PreGenesis, 0 },
        { 10, ProtocolEra::PreGenesis, 1 },
        { 19, ProtocolEra::PreGenesis, 5 },
        { 29, ProtocolEra::PreGenesis, 0x205 },
        { 39, ProtocolEra::PreGenesis, 0x605 },
        { 50, ProtocolEra::PreGenesis, 0x1'0607 },
        { 60, ProtocolEra::PreGenesis, 0x1'460f },
        { 60, ProtocolEra::PostGenesis, 0x5'462f },
        { 60, ProtocolEra::PostChronicle, 0x15'462f }
    };
    for (const auto [block_height, era, expected] : test_data)
    {
        Consensus::Params params;
        params.p2shHeight = 10;
        params.BIP66Height = 20;
        params.BIP65Height = 30;
        params.CSVHeight = 40;
        params.uahfHeight = 50;
        params.daaHeight = 60;
        const auto flags = GetBlockScriptFlags(params, block_height, era);
        BOOST_CHECK_EQUAL(expected, flags);
    }
}

// T22c: verify that the TeraTestNet / TeraScalingTestNet constructors wire
// p2sh / BIP65 / BIP66 / CSV into Consensus::Params so that GetBlockScriptFlags
// computes the right activation transitions at the custom-network boundaries.
//
// Flag bits (from script_flags.h):
//   P2SH=0x1, STRICTENC=0x2, DERSIG=0x4, LOW_S=0x8, BIP65=0x200, CSV=0x400,
//   NULLFAIL=0x4000, SIGHASH_FORKID=0x10000.
BOOST_AUTO_TEST_CASE(custom_chainparams_script_flag_boundaries)
{
    using namespace std;
    using test_data_type = tuple<int32_t,   // block height
                                 uint32_t>; // expected flags (PreGenesis era)

    // TeraTestNetParams: BIP65 / BIP66 / CSV all 0 (copied from teranode),
    // uahf = daa = 0, p2shHeight = 519 (from svnode CTestNetParams).
    // With BIP65/66/CSV at 0, `(height + 1) >= 0` is always true, so they are
    // always active. The only visible activation boundary is P2SH at 519.
    //   Below P2SH: STRICTENC|DERSIG|LOW_S|CLTV|CSV|NULLFAIL|SIGHASH_FORKID = 0x1'460E.
    //   At/above P2SH: 0x1'460F.
    {
        const auto params = bsv::CreateCustomChainParams(CustomChainParams::TERATESTNET);
        BOOST_REQUIRE(params != nullptr);
        const auto& consensus = params->GetConsensus();

        const vector<test_data_type> test_data{
            {      0, 0x1'460E },   // pre-P2SH
            {    518, 0x1'460E },   // one below P2SH boundary
            {    519, 0x1'460F },   // at P2SH boundary
            {1000000, 0x1'460F },   // well past P2SH
        };
        for (const auto& [block_height, expected] : test_data)
        {
            const auto flags = GetBlockScriptFlags(consensus, block_height, ProtocolEra::PreGenesis);
            BOOST_CHECK_EQUAL(expected, flags);
        }
    }

    // TeraScalingTestNetParams: BIP65 / BIP66 / CSV all 0 (copied from teranode),
    // uahf = daa = 0, p2shHeight = 1 (from svnode CStnParams).
    // Same always-on base as teratestnet; only the P2SH boundary differs (at 1).
    {
        const auto params = bsv::CreateCustomChainParams(CustomChainParams::TERASCALINGTESTNET);
        BOOST_REQUIRE(params != nullptr);
        const auto& consensus = params->GetConsensus();

        const vector<test_data_type> test_data{
            { 0, 0x1'460E },   // pre-P2SH (height 0 < p2shHeight 1)
            { 1, 0x1'460F },   // at P2SH boundary
            { 2, 0x1'460F },   // past P2SH
        };
        for (const auto& [block_height, expected] : test_data)
        {
            const auto flags = GetBlockScriptFlags(consensus, block_height, ProtocolEra::PreGenesis);
            BOOST_CHECK_EQUAL(expected, flags);
        }
    }
}

BOOST_AUTO_TEST_CASE(custom_genesis_height)
{
    const int32_t h{ 1000 };
    bsv::CTxValidator se("main");
    std::string err;
    const bool ok = se.SetGenesisActivationHeight(h, &err);
    BOOST_CHECK(ok);
    BOOST_CHECK(err.empty());
    const auto gh = se.GetGenesisActivationHeight();
    BOOST_CHECK(h == gh);
}

BOOST_AUTO_TEST_CASE(test_verify_script)
{
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the beta-8
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    std::span<const int32_t> utxo(utxoArray);
    bsv::CTxValidator se("main");
    const auto status = se.ValidateTransaction(etx, utxo, blockHeight, true);
    BOOST_CHECK(bsv::TxErrorIsOk(status));
}

BOOST_AUTO_TEST_CASE(test_verify_script_custom_flags)
{
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the beta-8
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    std::array<uint32_t, 2> customFlagsArray = { 869935, 869935 };
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    std::span<const int32_t> utxo(utxoArray);
    std::span<const uint32_t> customFlags(customFlagsArray);
    bsv::CTxValidator se("main");
    const auto status = se.VerifyScript(etx, utxo, blockHeight, true, customFlags);  // VerifyScript with custom flags stays as-is
    BOOST_CHECK(bsv::TxErrorIsOk(status));
}

BOOST_AUTO_TEST_CASE(test_verify_empty_utxos)
{
    // A extended transaction with zero input is expected to failed
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the beta-8
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::span<const int32_t> utxo;
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());

    CMutableTransactionExtended eTX;
    {
        const char* beginEtx{ reinterpret_cast<const char*>(etx.data()) };
        const char* endEtx{ reinterpret_cast<const char*>(etx.data() + etx.size()) };
        CDataStream tx_stream(beginEtx, endEtx, SER_NETWORK, PROTOCOL_VERSION);
        tx_stream >> eTX;
    }

    // Make the extended tx empty list of utxo
    eTX.vutxo.clear();
    eTX.mtx.vin.clear();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTX;
    // Convert stream to std::vector<uint8_t>
    std::vector<uint8_t> emptyUtxoEtxBin(ss.begin(), ss.end());
    const std::span<const uint8_t> emptyUtxoEtx(emptyUtxoEtxBin.data(), emptyUtxoEtxBin.size());

    bsv::CTxValidator se("main");
    const auto status = se.ValidateTransaction(emptyUtxoEtx, utxo, blockHeight, true);
    BOOST_CHECK(!bsv::TxErrorIsOk(status));
}

// Regression test for the multi-hour validator hang observed on BSV testnet
// tx 7bc9a3408dd0c87b835c887a0bce22c20788fc3c4b953929d4367656d80acab5, whose
// input spends a 490,001-byte locking script of
// (OP_2DUP OP_CHECKSIGVERIFY) * 245,000 + OP_CHECKSIG. With the cache, the
// production tx validates in a few minutes instead of multiple hours.
//
// The script performs 245,001 identical signature verifications because each
// OP_2DUP duplicates the [sig, pubkey] pair just popped by OP_CHECKSIGVERIFY.
// Without the per-input CachingScriptChecker (file-local helper in
// core/txvalidator.cpp) every iteration runs a full ECDSA verify plus a full
// SignatureHash that SHA256-streams the entire scriptCode buffer, driving
// wall-clock validation into the hours-on-fast-hardware range.
//
// The test reproduces the shape with a smaller N (TEST_N below) so that the
// without-cache path is observably slow but still finishes within CI budget,
// and asserts the with-cache path returns in well under one second. The
// assertion deadline is intentionally generous to absorb runner jitter while
// still catching a true hang or accidental disablement of the cache.
BOOST_AUTO_TEST_CASE(test_repeated_checksig_cache)
{
    using namespace std::chrono;

    constexpr size_t TEST_N = 10000; // (OP_2DUP OP_CHECKSIGVERIFY) repetitions
    constexpr int32_t blockHeight = 700000; // post-Genesis on mainnet
    constexpr int64_t prevAmountSatoshis = 218;

    // Deterministic key (same pattern as script_tests_modified.cpp::KeyData).
    constexpr std::array<uint8_t, 32> rawKey = {
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x7a };
    CKey key;
    key.Set(rawKey.begin(), rawKey.end(), /*fCompressedIn=*/true);
    BOOST_REQUIRE(key.IsValid());
    const CPubKey pubkey = key.GetPubKey();
    BOOST_REQUIRE(pubkey.IsValid());

    // Build the pathological locking script.
    CScript lockingScript;
    for (size_t i = 0; i < TEST_N; ++i) {
        lockingScript << OP_2DUP << OP_CHECKSIGVERIFY;
    }
    lockingScript << OP_CHECKSIG;

    // Build a minimal extended tx that spends a single previous output whose
    // scriptPubKey is the locking script above. Output is a dust-OP_RETURN to
    // keep the tx well-formed without needing a destination script.
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.nLockTime = 0;
    eTx.mtx.vin.resize(1);
    eTx.mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    eTx.mtx.vin[0].nSequence = 0xffffffff;
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(0);
    eTx.mtx.vout[0].scriptPubKey = CScript() << OP_FALSE << OP_RETURN;
    eTx.vutxo.resize(1);
    eTx.vutxo[0].nValue = Amount(prevAmountSatoshis);
    eTx.vutxo[0].scriptPubKey = lockingScript;

    // Sign using BIP143 (post-Genesis SIGHASH_FORKID path).
    const SigHashType sigHashType = SigHashType().withForkId();
    const uint256 sighash = SignatureHash(lockingScript,
                                          CTransaction(eTx.mtx),
                                          /*nIn=*/0,
                                          sigHashType,
                                          Amount(prevAmountSatoshis));
    std::vector<uint8_t> sig;
    BOOST_REQUIRE(key.Sign(sighash, sig));
    sig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));

    // Populate the unlocking script: push sig then pubkey.
    eTx.mtx.vin[0].scriptSig = CScript() << sig
                                         << ToByteVector(pubkey);

    // Serialise into the extended-tx wire form expected by VerifyScript.
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    std::vector<uint8_t> etxBin(ss.begin(), ss.end());

    bsv::CTxValidator se(CBaseChainParams::MAIN);
    // Allow the large locking script under policy as well as consensus.
    {
        std::string err;
        BOOST_REQUIRE(se.SetMaxScriptSizePolicy(static_cast<int64_t>(lockingScript.size()) + 1, &err));
        BOOST_REQUIRE(se.SetMaxOpsPerScriptPolicy(static_cast<int64_t>(TEST_N * 2 + 10), &err));
    }

    const std::array<int32_t, 1> utxoArray{ blockHeight - 1 };
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    const std::span<const int32_t> utxo(utxoArray);

    const auto t0 = steady_clock::now();
    const auto status = se.VerifyScript(etx, utxo, blockHeight, /*consensus=*/true);
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - t0).count();

    // Emit to stderr unconditionally so ctest --output-on-failure surfaces the
    // measurement on regression. BOOST_TEST_MESSAGE is suppressed at default
    // log levels and is not visible in BDK's current CI output.
    std::cerr << "[test_repeated_checksig_cache] N=" << TEST_N
              << " script_bytes=" << lockingScript.size()
              << " status_ok=" << (bsv::TxErrorIsOk(status) ? "true" : "false")
              << " elapsed_ms=" << elapsed << std::endl;

    BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
        "VerifyScript returned non-OK status");
    // Without the per-instance CheckSig cache this verify takes seconds-to-minutes
    // on CI hardware. With the cache it completes in well under 1 s; budget at
    // 5 s to absorb runner jitter while still catching disablement of the cache.
    BOOST_CHECK_MESSAGE(elapsed < 5000,
        "VerifyScript took " << elapsed << " ms with N=" << TEST_N
        << " — cache likely not hitting (expected sub-second with cache enabled)");
}

// Consolidation-policy setters: verify svnode-faithful validation
// (reject negative; 0 → DEFAULT for MaxConsolidationInputScriptSize and
// MinConfConsolidationInput; literal 0 for MinConsolidationFactor).
BOOST_AUTO_TEST_CASE(consolidation_setters_svnode_faithful_validation)
{
    bsv::CTxValidator se("main");
    std::string err;

    // MinConsolidationFactor: 0 stored literally (disables consolidation);
    // positive stored verbatim; negative rejected.
    err.clear();
    BOOST_CHECK(se.SetMinConsolidationFactor(0, &err));
    BOOST_CHECK_EQUAL(uint64_t{0}, se.GetMinConsolidationFactor());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(se.SetMinConsolidationFactor(50, &err));
    BOOST_CHECK_EQUAL(uint64_t{50}, se.GetMinConsolidationFactor());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(!se.SetMinConsolidationFactor(-1, &err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(uint64_t{50}, se.GetMinConsolidationFactor()); // unchanged

    // MaxConsolidationInputScriptSize: 0 → 150 (svnode default); positive verbatim;
    // negative rejected.
    err.clear();
    BOOST_CHECK(se.SetMaxConsolidationInputScriptSize(0, &err));
    BOOST_CHECK_EQUAL(uint64_t{150}, se.GetMaxConsolidationInputScriptSize());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(se.SetMaxConsolidationInputScriptSize(500, &err));
    BOOST_CHECK_EQUAL(uint64_t{500}, se.GetMaxConsolidationInputScriptSize());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(!se.SetMaxConsolidationInputScriptSize(-1, &err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(uint64_t{500}, se.GetMaxConsolidationInputScriptSize()); // unchanged

    // MinConfConsolidationInput: 0 → 6 (svnode default); positive verbatim;
    // negative rejected.
    err.clear();
    BOOST_CHECK(se.SetMinConfConsolidationInput(0, &err));
    BOOST_CHECK_EQUAL(uint64_t{6}, se.GetMinConfConsolidationInput());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(se.SetMinConfConsolidationInput(100, &err));
    BOOST_CHECK_EQUAL(uint64_t{100}, se.GetMinConfConsolidationInput());
    BOOST_CHECK(err.empty());

    err.clear();
    BOOST_CHECK(!se.SetMinConfConsolidationInput(-1, &err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(uint64_t{100}, se.GetMinConfConsolidationInput()); // unchanged

    // AcceptNonStdConsolidationInput: bool toggle, no error channel.
    se.SetAcceptNonStdConsolidationInput(true);
    BOOST_CHECK(se.GetAcceptNonStdConsolidationInput());
    se.SetAcceptNonStdConsolidationInput(false);
    BOOST_CHECK(!se.GetAcceptNonStdConsolidationInput());
}

// SetMinMiningTxFee setter: validate negative rejection + value storage.
BOOST_AUTO_TEST_CASE(min_mining_tx_fee_setter_validation)
{
    bsv::CTxValidator se("main");
    std::string err;

    // Default is 0 (no fee policy).
    BOOST_CHECK_EQUAL(int64_t{0}, se.GetMinMiningTxFee());

    // Positive accepted.
    err.clear();
    BOOST_CHECK(se.SetMinMiningTxFee(500, &err));
    BOOST_CHECK_EQUAL(int64_t{500}, se.GetMinMiningTxFee());
    BOOST_CHECK(err.empty());

    // Zero accepted (means "no fee policy").
    err.clear();
    BOOST_CHECK(se.SetMinMiningTxFee(0, &err));
    BOOST_CHECK_EQUAL(int64_t{0}, se.GetMinMiningTxFee());
    BOOST_CHECK(err.empty());

    // Negative rejected; previous value preserved.
    err.clear();
    BOOST_CHECK(se.SetMinMiningTxFee(1000, &err));
    BOOST_CHECK_EQUAL(int64_t{1000}, se.GetMinMiningTxFee());

    err.clear();
    BOOST_CHECK(!se.SetMinMiningTxFee(-1, &err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(int64_t{1000}, se.GetMinMiningTxFee()); // unchanged
}

// Fee-path inside ValidateTransaction (policy mode).
// Uses a real mainnet whitelisted tx (the same fixture as test_verify_script).
// Its actual fee = inputs(622,674,087 + 3,127,348) - output(625,132,423) = 669,012 sats.
// Wire size is small (~370 B), so fee-rate sensitivity tests below select rates that
// cleanly straddle the implied minimum.
BOOST_AUTO_TEST_CASE(check_fee_policy_path)
{
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    const std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    const std::span<const int32_t> utxo(utxoArray);

    // Consensus mode never runs the fee check — even with an extreme rate set, accept.
    {
        bsv::CTxValidator se("main");
        std::string err;
        BOOST_REQUIRE(se.SetMinMiningTxFee(1'000'000'000'000LL, &err));
        const auto status = se.ValidateTransaction(etx, utxo, blockHeight, /*consensus=*/true);
        BOOST_CHECK(bsv::TxErrorIsOk(status));
    }

    // Policy mode, fee rate = 0 (no policy) — accept regardless of tx fees.
    {
        bsv::CTxValidator se("main");
        const auto status = se.ValidateTransaction(etx, utxo, blockHeight, /*consensus=*/false);
        BOOST_CHECK(bsv::TxErrorIsOk(status));
    }

    // Policy mode, low fee rate — well below the tx's actual 669,012 sat fee → accept.
    {
        bsv::CTxValidator se("main");
        std::string err;
        BOOST_REQUIRE(se.SetMinMiningTxFee(500, &err));  // 0.5 sat/byte
        const auto status = se.ValidateTransaction(etx, utxo, blockHeight, /*consensus=*/false);
        BOOST_CHECK(bsv::TxErrorIsOk(status));
    }

    // Policy mode, extreme fee rate — tx's actual fee is far below the implied floor,
    // and the tx is not a free consolidation → reject with InsufficientFee.
    {
        bsv::CTxValidator se("main");
        std::string err;
        BOOST_REQUIRE(se.SetMinMiningTxFee(1'000'000'000'000LL, &err));
        const auto status = se.ValidateTransaction(etx, utxo, blockHeight, /*consensus=*/false);
        BOOST_CHECK(!bsv::TxErrorIsOk(status));
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_DOS), status.domain);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(bsv::DoSError_t::InsufficientFee), status.code);
    }
}

// Bypass path: under the fee floor but qualifies as a dust-return (donation)
// free consolidation → fee check does NOT reject.
//
// Construction: take the same mainnet fixture, replace its outputs with a single
// IsDustReturnScript output (the 7-byte sequence OP_FALSE OP_RETURN OP_PUSHDATA4
// 'dust'). This makes implIsFreeConsolidation's isDonation branch trigger:
//   factor   = tx.vin.size() (= 2)         → vin.size() >= factor*vout.size(): 2 >= 2*1 ✓
//   minConf  = 0                            → confirmation checks skipped
//   ratio    = sumInputScriptPubKey (46 B) >= factor*sumOutputScriptPubKey (2*7=14 B) ✓
//
// Caveat: the fixture's prev outputs are P2SH and the test runs at post-Genesis
// height (632099 > GENESIS_ACTIVATION_MAIN = 620538). Post-Genesis, P2SH is no
// longer a standard output type, so IsStandardOutput on the prev UTXOs would
// reject them inside IsFreeConsolidation. We sidestep that by setting
// AcceptNonStdConsolidationInput = true (matches bitcoin-sv
// `-acceptnonstdconsolidationinput=1`). The point of this test is the fee-floor
// gate, not the standardness sub-check (that is exercised elsewhere).
//
// Same-fixture control to isolate the gate as the cause of the outcome
// difference (defends against the "validation fails earlier" false-pass):
//   (A) SetMinConsolidationFactor(0)  → IsFreeConsolidation short-circuits to
//       NotFreeConsolidation at the very first line, so the fee-floor reject path
//       runs → expect status == InsufficientFee.
//   (B) Default minFactor (20, > 0)   → the dust-return donation branch qualifies,
//       gate opens → expect status != InsufficientFee (later script verify fails
//       because the scriptSigs no longer match the modified outputs, but that
//       surfaces in a different domain).
// Only the consolidation-gate setting differs between the two runs, so whatever
// makes (A) and (B) diverge is the gate logic.
BOOST_AUTO_TEST_CASE(check_fee_free_consolidation_bypass)
{
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    const std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);

    // Deserialize, swap the output for an IsDustReturnScript donation, re-serialize.
    CMutableTransactionExtended eTX;
    {
        const char* beginEtx{ reinterpret_cast<const char*>(etxBin.data()) };
        const char* endEtx{ reinterpret_cast<const char*>(etxBin.data() + etxBin.size()) };
        CDataStream tx_stream(beginEtx, endEtx, SER_NETWORK, PROTOCOL_VERSION);
        tx_stream >> eTX;
    }

    // IsDustReturnScript-recognised 7-byte sequence: OP_FALSE OP_RETURN OP_PUSHDATA(4) 'dust'.
    CScript dustReturnScript;
    dustReturnScript << OP_FALSE << OP_RETURN;
    dustReturnScript.push_back(0x04);
    dustReturnScript.push_back('d');
    dustReturnScript.push_back('u');
    dustReturnScript.push_back('s');
    dustReturnScript.push_back('t');

    eTX.mtx.vout.clear();
    eTX.mtx.vout.emplace_back(Amount(0), dustReturnScript);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTX;
    const std::vector<uint8_t> donationBin(ss.begin(), ss.end());
    const std::span<const uint8_t> donationEtx(donationBin.data(), donationBin.size());
    const std::span<const int32_t> utxo(utxoArray);

    auto runOnce = [&](uint64_t minConsolidationFactor) {
        bsv::CTxValidator se("main");
        std::string err;
        BOOST_REQUIRE(se.SetMinMiningTxFee(1'000'000'000'000LL, &err));
        se.SetAcceptNonStdConsolidationInput(true);  // see test comment re: P2SH post-Genesis
        BOOST_REQUIRE(se.SetMinConsolidationFactor(static_cast<int64_t>(minConsolidationFactor), &err));
        return se.ValidateTransaction(donationEtx, utxo, blockHeight, /*consensus=*/false);
    };

    const auto isInsufficientFee = [](TxError s) {
        return (s.domain == static_cast<int32_t>(TX_ERR_DOMAIN_DOS))
            && (s.code   == static_cast<int32_t>(bsv::DoSError_t::InsufficientFee));
    };

    // (A) Gate disabled: same tx, same rate, but consolidationMinFactor=0 short-circuits
    // implIsFreeConsolidation to NotFreeConsolidation → fee floor reject must fire.
    const auto disabled = runOnce(/*minConsolidationFactor=*/0);
    BOOST_CHECK_MESSAGE(isInsufficientFee(disabled),
        "Control case failed: with consolidation disabled (factor=0) the tx should hit "
        "the fee-floor reject, but did not return InsufficientFee.");

    // (B) Gate enabled (default factor=20): dust-return donation qualifies, fee floor
    // bypassed → must NOT see InsufficientFee. Any later validation failure is fine; it
    // just must not be the fee-floor reject from this code path.
    const auto enabled = runOnce(/*minConsolidationFactor=*/20);
    BOOST_CHECK_MESSAGE(!isInsufficientFee(enabled),
        "Free-consolidation bypass failed: tx returned InsufficientFee despite "
        "qualifying as a dust-return donation.");
}

// The ABI error domain belongs to the cgo shim, not to the core validator. An empty
// extended transaction is a parse failure and must keep returning the exception
// domain: nothing moved out of TX_ERR_DOMAIN_EXCEPTION when TX_ERR_DOMAIN_ABI was
// added.
BOOST_AUTO_TEST_CASE(empty_extended_tx_stays_in_the_exception_domain)
{
    const std::span<const uint8_t> emptyEtx;
    const std::span<const int32_t> utxo;

    bsv::CTxValidator se("main");
    const auto status = se.ValidateTransaction(emptyEtx, utxo, /*blockHeight=*/632099, /*consensus=*/true);
    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION), status.domain);
}

// Builds the low-opcount spendable form that places bulk data in an unexecuted
// conditional: OP_FALSE OP_IF OP_PUSHDATA4 <payload> OP_ENDIF OP_TRUE, sized to
// exactly totalSize bytes. The push is written by hand because the payload size, not
// the encoding, is what the test is about.
static CScript MakeConditionalDataScript(size_t totalSize)
{
    constexpr size_t wrapper = 9; // OP_FALSE OP_IF OP_PUSHDATA4 <4 length bytes> ... OP_ENDIF OP_TRUE
    const size_t payload = totalSize - wrapper;

    std::vector<uint8_t> raw;
    raw.reserve(totalSize);
    raw.push_back(OP_FALSE);
    raw.push_back(OP_IF);
    raw.push_back(OP_PUSHDATA4);
    for (size_t i = 0; i < 4; ++i) {
        raw.push_back(static_cast<uint8_t>((payload >> (8 * i)) & 0xff));
    }
    raw.insert(raw.end(), payload, uint8_t{ 0x01 });
    raw.push_back(OP_ENDIF);
    raw.push_back(OP_TRUE);

    return CScript(raw.begin(), raw.end());
}

// A transaction whose bare size is a couple of hundred bytes but whose aggregate
// previous locking scripts are megabytes — the shape the issue describes, bounded so
// the test costs about 8 MB rather than 2.16 GB. The validator must reach a verdict in
// both modes: neither the exception domain nor the ABI domain may appear.
BOOST_AUTO_TEST_CASE(small_bare_large_aggregate_previous_scripts)
{
    // Sizes crossing the 0xfc / 0xfd / 0xffff / 0x10000 CompactSize thresholds, plus
    // one multi-megabyte script.
    const std::array<size_t, 5> scriptSizes{ 252, 253, 0xffff, 0x10000, 8000000 };
    constexpr int32_t blockHeight = 800000;  // post-Genesis on mainnet
    constexpr int32_t utxoHeight = 700000;

    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.nLockTime = 0;
    eTx.mtx.vin.resize(scriptSizes.size());
    eTx.vutxo.resize(scriptSizes.size());

    for (size_t i = 0; i < scriptSizes.size(); ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("01"), static_cast<uint32_t>(i));
        eTx.mtx.vin[i].nSequence = 0xffffffff;
        eTx.vutxo[i].nValue = Amount(1000);
        eTx.vutxo[i].scriptPubKey = MakeConditionalDataScript(scriptSizes[i]);
        BOOST_REQUIRE_EQUAL(scriptSizes[i], static_cast<size_t>(eTx.vutxo[i].scriptPubKey.size()));
    }

    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(1);
    eTx.mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    const std::vector<uint8_t> etxBin(ss.begin(), ss.end());
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());

    const std::vector<int32_t> utxoHeights(scriptSizes.size(), utxoHeight);
    const std::span<const int32_t> utxo(utxoHeights);

    bsv::CTxValidator se("main");
    for (const bool consensus : { true, false }) {
        const auto status = se.ValidateTransaction(etx, utxo, blockHeight, consensus);
        BOOST_CHECK_MESSAGE(status.domain != static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
            "consensus=" << consensus << ": a large aggregate of previous scripts must give a "
            "verdict, not the exception domain");
        BOOST_CHECK_MESSAGE(status.domain != static_cast<int32_t>(TX_ERR_DOMAIN_ABI),
            "consensus=" << consensus << ": the ABI domain must never appear in the core validator");
    }
}

// Synthetic P2PKH spends for the sighash tests below. Every input spends
// COutPoint(01, i), worth P2PKH_INPUT_SATOSHIS and locked to one deterministic
// key; output j pays P2PKH_OUTPUT_SATOSHIS + j back to the same key. Output
// values and nSequence differ by position, so sighash data built from a
// reordered tx no longer hashes the same.
constexpr int64_t P2PKH_INPUT_SATOSHIS = 100000;
constexpr int64_t P2PKH_OUTPUT_SATOSHIS = 1000;
constexpr int32_t P2PKH_BLOCK_HEIGHT = 700000;  // post-Genesis, pre-Chronicle on mainnet
constexpr int32_t P2PKH_UTXO_HEIGHT = 699999;

static CKey MakeDeterministicTestKey(uint8_t lastByte, uint8_t secondLastByte = 0)
{
    std::array<uint8_t, 32> rawKey{};
    rawKey[30] = secondLastByte;
    rawKey[31] = lastByte;
    CKey key;
    key.Set(rawKey.begin(), rawKey.end(), /*fCompressedIn=*/true);
    return key;
}

static CKey MakeP2PKHTestKey()
{
    return MakeDeterministicTestKey(0x64);
}

static CScript MakeP2PKHScript(const CPubKey& pubkey)
{
    return CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID())
                     << OP_EQUALVERIFY << OP_CHECKSIG;
}

// Builds and signs an extended tx with one input per entry of sigHashTypes.
// precomputeForSigning only changes how the signer reaches the sighash: without
// it every input is signed through SignatureHash with no shared data, which is
// independent of the precompute the validator uses; with it signing a large tx
// stays linear. forkIdDigest is SignatureHash's enabledSighashForkid: false
// signs a FORKID-bit type over the original digest, as before the UAHF fork.
static bsv::CMutableTransactionExtended MakeSignedP2PKHSpend(
    const std::vector<SigHashType>& sigHashTypes,
    size_t nOutputs,
    bool precomputeForSigning = false,
    bool forkIdDigest = true)
{
    const CKey key = MakeP2PKHTestKey();
    const CPubKey pubkey = key.GetPubKey();
    const CScript lockingScript = MakeP2PKHScript(pubkey);
    const Amount inputAmount(P2PKH_INPUT_SATOSHIS);

    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.nLockTime = 0;
    eTx.mtx.vin.resize(sigHashTypes.size());
    eTx.vutxo.resize(sigHashTypes.size());
    for (size_t i = 0; i < sigHashTypes.size(); ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("01"), static_cast<uint32_t>(i));
        // Bit 31 stays set, so BIP68 relative locktime never applies; nLockTime 0
        // keeps the tx final whatever the sequence.
        eTx.mtx.vin[i].nSequence = 0xffffffff - static_cast<uint32_t>(i % 3);
        eTx.vutxo[i].nValue = inputAmount;
        eTx.vutxo[i].scriptPubKey = lockingScript;
    }
    eTx.mtx.vout.resize(nOutputs);
    for (size_t j = 0; j < nOutputs; ++j) {
        eTx.mtx.vout[j].nValue = Amount(P2PKH_OUTPUT_SATOSHIS + static_cast<int64_t>(j));
        eTx.mtx.vout[j].scriptPubKey = lockingScript;
    }

    // BIP143 never commits to scriptSigs, so every input can be signed against
    // the unsigned tx.
    const CTransaction unsignedTx(eTx.mtx);
    std::optional<PrecomputedTransactionData> txdata;
    if (precomputeForSigning)
        txdata.emplace(unsignedTx);

    for (size_t i = 0; i < sigHashTypes.size(); ++i) {
        const uint256 sighash = SignatureHash(lockingScript, unsignedTx, static_cast<unsigned int>(i),
            sigHashTypes[i], inputAmount, txdata ? &*txdata : nullptr, forkIdDigest);
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(key.Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(sigHashTypes[i].getRawSigHashType()));
        eTx.mtx.vin[i].scriptSig = CScript() << sig << ToByteVector(pubkey);
    }
    return eTx;
}

static std::vector<uint8_t> SerializeExtended(const bsv::CMutableTransactionExtended& eTx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return std::vector<uint8_t>(ss.begin(), ss.end());
}

static TxError ValidateP2PKHSpend(const bsv::CTxValidator& se,
                                  const std::vector<uint8_t>& etxBin,
                                  size_t nInputs,
                                  bool consensus)
{
    const std::vector<int32_t> utxoHeights(nInputs, P2PKH_UTXO_HEIGHT);
    return se.ValidateTransaction(std::span<const uint8_t>(etxBin),
        std::span<const int32_t>(utxoHeights), P2PKH_BLOCK_HEIGHT, consensus);
}

// Flips the low bit of the last byte of r in the DER signature pushed by input
// `index`, keeping the encoding strict-DER and low-S so only ECDSA verification
// can reject it.
static void CorruptP2PKHSignature(bsv::CMutableTransactionExtended& eTx, size_t index)
{
    CScript::const_iterator pc = eTx.mtx.vin[index].scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> sig;
    std::vector<uint8_t> pubkey;
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, sig));
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, pubkey));
    BOOST_REQUIRE(sig.size() > 5 && sig[0] == 0x30 && sig[2] == 0x02);
    const size_t rLen = sig[3];
    sig[4 + rLen - 1] ^= 0x01;
    eTx.mtx.vin[index].scriptSig = CScript() << sig << pubkey;
}

static const std::array<SigHashType, 6> FORKID_SIGHASH_TYPES{
    SigHashType().withBaseType(BaseSigHashType::ALL).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::NONE).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::ALL).withForkId().withAnyoneCanPay(),
    SigHashType().withBaseType(BaseSigHashType::NONE).withForkId().withAnyoneCanPay(),
    SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId().withAnyoneCanPay(),
};

// Each FORKID sighash type, with and without ANYONECANPAY, on every input of a
// multi-input, multi-output tx. SINGLE has a matching output for every input.
BOOST_AUTO_TEST_CASE(forkid_sighash_types_validate_on_multi_input_tx)
{
    const bsv::CTxValidator se("main");
    for (const SigHashType& type : FORKID_SIGHASH_TYPES) {
        const std::vector<SigHashType> types(3, type);
        const auto etxBin = SerializeExtended(MakeSignedP2PKHSpend(types, /*nOutputs=*/3));
        for (const bool consensus : { true, false }) {
            const auto status = ValidateP2PKHSpend(se, etxBin, types.size(), consensus);
            BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
                "sighash type 0x" << std::hex << type.getRawSigHashType() << std::dec
                << " consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
        }
    }
}

// All six FORKID sighash types mixed across the inputs of one tx.
BOOST_AUTO_TEST_CASE(mixed_forkid_sighash_types_validate_in_one_tx)
{
    const bsv::CTxValidator se("main");
    const std::vector<SigHashType> types(FORKID_SIGHASH_TYPES.begin(), FORKID_SIGHASH_TYPES.end());
    const auto etxBin = SerializeExtended(MakeSignedP2PKHSpend(types, /*nOutputs=*/types.size()));
    for (const bool consensus : { true, false }) {
        const auto status = ValidateP2PKHSpend(se, etxBin, types.size(), consensus);
        BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
            "consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
    }
}

// A bad signature on any one input fails the tx with the script error, wherever
// that input sits.
BOOST_AUTO_TEST_CASE(corrupted_signature_fails_at_first_middle_and_last_input)
{
    constexpr size_t nInputs = 5;
    const bsv::CTxValidator se("main");
    const std::vector<SigHashType> types(nInputs, SigHashType().withForkId());
    for (const size_t k : { size_t{ 0 }, nInputs / 2, nInputs - 1 }) {
        auto eTx = MakeSignedP2PKHSpend(types, /*nOutputs=*/2);
        CorruptP2PKHSignature(eTx, k);
        const auto etxBin = SerializeExtended(eTx);
        for (const bool consensus : { true, false }) {
            const auto status = ValidateP2PKHSpend(se, etxBin, nInputs, consensus);
            BOOST_CHECK_MESSAGE(status.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT),
                "k=" << k << " consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
            // A non-empty signature that fails ECDSA trips NULLFAIL, which these
            // flags set in both modes, before OP_CHECKSIG can push false.
            BOOST_CHECK_MESSAGE(status.code == static_cast<int32_t>(SCRIPT_ERR_SIG_NULLFAIL),
                "k=" << k << " consensus=" << consensus << ": code=" << status.code);
        }
    }
}

// SIGHASH_SINGLE|FORKID on an input with no output at the same index. BIP143
// signs a zero hashOutputs in that case (no "sighash 1" quirk), so the
// signature validates. Pins SV Node's behaviour as BDK reproduces it.
BOOST_AUTO_TEST_CASE(sighash_single_without_matching_output)
{
    const bsv::CTxValidator se("main");
    const SigHashType all = SigHashType().withForkId();
    const SigHashType single = SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId();
    const std::vector<SigHashType> types{ all, all, single };
    const auto etxBin = SerializeExtended(MakeSignedP2PKHSpend(types, /*nOutputs=*/2));
    for (const bool consensus : { true, false }) {
        const auto status = ValidateP2PKHSpend(se, etxBin, types.size(), consensus);
        BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
            "consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
    }
}

// The guard txs above with input `index` re-locked to a bare 2-of-3
// OP_CHECKMULTISIG and signed by the first two of its three keys, ALL|FORKID.
// BIP143 commits only to an input's own scriptCode and amount, so the other
// inputs' P2PKH signatures stay valid.
struct MultisigSpend {
    bsv::CMutableTransactionExtended eTx;
    size_t index;
};

static MultisigSpend MakeSpendWithMultisigInput(size_t nInputs, size_t index, size_t nOutputs)
{
    const SigHashType allForkId = SigHashType().withForkId();
    MultisigSpend spend{ MakeSignedP2PKHSpend(std::vector<SigHashType>(nInputs, allForkId), nOutputs), index };

    const std::array<CKey, 3> keys{ MakeDeterministicTestKey(0x65), MakeDeterministicTestKey(0x66),
                                    MakeDeterministicTestKey(0x67) };
    CScript multisig = CScript() << OP_2;
    for (const CKey& key : keys)
        multisig << ToByteVector(key.GetPubKey());
    multisig << OP_3 << OP_CHECKMULTISIG;
    spend.eTx.vutxo[index].scriptPubKey = multisig;

    const CTransaction unsignedTx(spend.eTx.mtx);
    const uint256 sighash = SignatureHash(multisig, unsignedTx, static_cast<unsigned int>(index),
        allForkId, spend.eTx.vutxo[index].nValue);
    CScript scriptSig = CScript() << OP_0;  // CHECKMULTISIG's extra pop; NULLDUMMY wants it empty
    for (size_t k = 0; k < 2; ++k) {
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(keys[k].Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(allForkId.getRawSigHashType()));
        scriptSig << sig;
    }
    spend.eTx.mtx.vin[index].scriptSig = scriptSig;
    return spend;
}

// Flips the low bit of the last byte of r in the second multisig signature,
// keeping it strict-DER and low-S.
static void CorruptSecondMultisigSignature(MultisigSpend& spend)
{
    const CScript& scriptSig = spend.eTx.mtx.vin[spend.index].scriptSig;
    CScript::const_iterator pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> dummy, sig1, sig2;
    BOOST_REQUIRE(scriptSig.GetOp(pc, opcode, dummy));
    BOOST_REQUIRE(scriptSig.GetOp(pc, opcode, sig1));
    BOOST_REQUIRE(scriptSig.GetOp(pc, opcode, sig2));
    BOOST_REQUIRE(sig2.size() > 5 && sig2[0] == 0x30 && sig2[2] == 0x02);
    sig2[4 + sig2[3] - 1] ^= 0x01;
    spend.eTx.mtx.vin[spend.index].scriptSig = CScript() << OP_0 << sig1 << sig2;
}

static TxError VerifyMultisigSpend(const bsv::CTxValidator& se, const MultisigSpend& spend, bool consensus)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << spend.eTx.mtx;
    const std::vector<uint8_t> txBin(ss.begin(), ss.end());
    const CTxOut& source = spend.eTx.vutxo[spend.index];
    return se.VerifySpend(std::span<const uint8_t>(txBin), static_cast<uint32_t>(spend.index),
        std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
        source.nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT, consensus);
}

// Several FORKID checks in one input: a 2-of-3 bare multisig among P2PKH
// inputs, through VerifySpend on that input and ValidateTransaction on the tx.
BOOST_AUTO_TEST_CASE(multisig_input_validates_through_verify_spend_and_validate_transaction)
{
    constexpr size_t nInputs = 4;
    const bsv::CTxValidator se("main");
    for (const size_t index : { size_t{ 0 }, nInputs - 1 }) {
        const MultisigSpend spend = MakeSpendWithMultisigInput(nInputs, index, /*nOutputs=*/3);
        const auto etxBin = SerializeExtended(spend.eTx);
        for (const bool consensus : { true, false }) {
            const auto spendStatus = VerifyMultisigSpend(se, spend, consensus);
            BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(spendStatus),
                "VerifySpend index=" << index << " consensus=" << consensus
                << ": domain=" << spendStatus.domain << " code=" << spendStatus.code);
            const auto txStatus = ValidateP2PKHSpend(se, etxBin, nInputs, consensus);
            BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(txStatus),
                "ValidateTransaction index=" << index << " consensus=" << consensus
                << ": domain=" << txStatus.domain << " code=" << txStatus.code);
        }
    }
}

// One bad signature among the multisig's two fails the spend with the script
// error, through both entry points.
BOOST_AUTO_TEST_CASE(multisig_input_with_corrupted_signature_fails)
{
    constexpr size_t nInputs = 4;
    const bsv::CTxValidator se("main");
    MultisigSpend spend = MakeSpendWithMultisigInput(nInputs, /*index=*/1, /*nOutputs=*/3);
    CorruptSecondMultisigSignature(spend);
    const auto etxBin = SerializeExtended(spend.eTx);
    for (const bool consensus : { true, false }) {
        for (const auto& [entry, status] : { std::pair{ "VerifySpend", VerifyMultisigSpend(se, spend, consensus) },
                                             std::pair{ "ValidateTransaction", ValidateP2PKHSpend(se, etxBin, nInputs, consensus) } }) {
            BOOST_CHECK_MESSAGE(status.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT),
                entry << " consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
            BOOST_CHECK_MESSAGE(status.code == static_cast<int32_t>(SCRIPT_ERR_SIG_NULLFAIL),
                entry << " consensus=" << consensus << ": code=" << status.code);
        }
    }
}

// The precompute only feeds BIP143. Signatures that go to
// SignatureHashOriginal must validate exactly as before. Three routes lead
// there: FORKID|CHRONICLE (allowed from Chronicle on, beside BIP143 inputs in
// the same tx); no FORKID bit, which is only valid in consensus mode before the
// UAHF fork; and, also before the fork, a FORKID bit while consensus flags
// carry neither SCRIPT_ENABLE_SIGHASH_FORKID nor STRICTENC, so the bit is
// legal but the digest is the original one. From the fork on every signature
// must carry FORKID and use BIP143 (SCRIPT_ERR_MUST_USE_FORKID, after
// Chronicle too), and policy flags always require it.
constexpr int32_t POST_CHRONICLE_BLOCK_HEIGHT = 950000;  // mainnet Chronicle: 943,816
constexpr int32_t POST_CHRONICLE_UTXO_HEIGHT = 949999;
constexpr int32_t PRE_UAHF_BLOCK_HEIGHT = 470000;  // mainnet UAHF: 478,558
constexpr int32_t PRE_UAHF_UTXO_HEIGHT = 469999;

static TxError ValidateAtHeights(const bsv::CTxValidator& se,
                                 const bsv::CMutableTransactionExtended& eTx,
                                 int32_t utxoHeight,
                                 int32_t blockHeight,
                                 bool consensus)
{
    const auto etxBin = SerializeExtended(eTx);
    const std::vector<int32_t> utxoHeights(eTx.mtx.vin.size(), utxoHeight);
    return se.ValidateTransaction(std::span<const uint8_t>(etxBin),
        std::span<const int32_t>(utxoHeights), blockHeight, consensus);
}

// BIP143 and Chronicle (original-algorithm) signatures interleaved in one tx,
// after Chronicle: valid, and a corrupted Chronicle or BIP143 signature fails.
// After Chronicle a version-2 tx is malleable, NULLFAIL is not enforced, and
// the failed check ends the script false.
BOOST_AUTO_TEST_CASE(chronicle_and_forkid_signatures_mixed_in_one_tx)
{
    const bsv::CTxValidator se("main");
    const SigHashType forkId = SigHashType().withForkId();
    const SigHashType chronicle = forkId.withChronicle();  // 0x61
    const std::vector<SigHashType> types{
        forkId,
        chronicle,
        forkId.withBaseType(BaseSigHashType::SINGLE),
        chronicle.withBaseType(BaseSigHashType::SINGLE),
        chronicle.withAnyoneCanPay(),
        forkId.withBaseType(BaseSigHashType::NONE).withAnyoneCanPay(),
        chronicle.withBaseType(BaseSigHashType::NONE),
    };
    const auto valid = MakeSignedP2PKHSpend(types, /*nOutputs=*/types.size());
    for (const bool consensus : { true, false }) {
        const auto status = ValidateAtHeights(se, valid, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT, consensus);
        BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
            "consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
    }

    // Input 1 is Chronicle (original algorithm), input 2 BIP143.
    for (const size_t k : { size_t{ 1 }, size_t{ 2 } }) {
        auto corrupted = valid;
        CorruptP2PKHSignature(corrupted, k);
        for (const bool consensus : { true, false }) {
            const auto status = ValidateAtHeights(se, corrupted, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT, consensus);
            BOOST_CHECK_MESSAGE(status.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT),
                "k=" << k << " consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
            BOOST_CHECK_MESSAGE(status.code == static_cast<int32_t>(SCRIPT_ERR_EVAL_FALSE),
                "k=" << k << " consensus=" << consensus << ": code=" << status.code);
        }
    }
}

// Plain non-FORKID signatures on every input, before the UAHF fork in
// consensus mode: valid, and a corrupted one fails. Policy mode, and consensus
// after Chronicle, reject the same tx with MUST_USE_FORKID.
BOOST_AUTO_TEST_CASE(non_forkid_signatures_validate_before_uahf_only)
{
    const bsv::CTxValidator se("main");
    const std::vector<SigHashType> types{
        SigHashType(),
        SigHashType().withBaseType(BaseSigHashType::NONE),
        SigHashType().withBaseType(BaseSigHashType::SINGLE),
        SigHashType().withAnyoneCanPay(),
    };
    const auto valid = MakeSignedP2PKHSpend(types, /*nOutputs=*/types.size());
    const auto status = ValidateAtHeights(se, valid, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/true);
    BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
        "pre-UAHF consensus: domain=" << status.domain << " code=" << status.code);

    const auto mustUseForkId = [](const TxError& e) {
        return e.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT)
            && e.code == static_cast<int32_t>(SCRIPT_ERR_MUST_USE_FORKID);
    };
    const auto policy = ValidateAtHeights(se, valid, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/false);
    BOOST_CHECK_MESSAGE(mustUseForkId(policy), "pre-UAHF policy: domain=" << policy.domain << " code=" << policy.code);
    const auto late = ValidateAtHeights(se, valid, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true);
    BOOST_CHECK_MESSAGE(mustUseForkId(late), "post-Chronicle consensus: domain=" << late.domain << " code=" << late.code);

    // Before the fork NULLFAIL is not a consensus rule; the failed check ends
    // the script false.
    auto corrupted = valid;
    CorruptP2PKHSignature(corrupted, 2);
    const auto bad = ValidateAtHeights(se, corrupted, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/true);
    BOOST_CHECK_MESSAGE(bad.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT)
        && bad.code == static_cast<int32_t>(SCRIPT_ERR_EVAL_FALSE),
        "pre-UAHF consensus, corrupted: domain=" << bad.domain << " code=" << bad.code);
}

// Before the UAHF fork, in consensus mode, a FORKID-bit signature is checked
// over the original digest. Mixed with plain non-FORKID inputs, the tx
// validates when every input is signed over the original digest, and fails
// when the FORKID-bit inputs are signed over BIP143 instead. Policy flags
// require FORKID and BIP143, so the original-digest tx fails there.
BOOST_AUTO_TEST_CASE(forkid_bit_uses_original_digest_before_uahf)
{
    const bsv::CTxValidator se("main");
    const std::vector<SigHashType> types{
        SigHashType(),                                                                   // 0x01
        SigHashType().withForkId(),                                                      // 0x41
        SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId().withAnyoneCanPay(), // 0xc3
        SigHashType().withBaseType(BaseSigHashType::NONE).withForkId(),                  // 0x42
    };

    const auto original = MakeSignedP2PKHSpend(types, /*nOutputs=*/types.size(),
        /*precomputeForSigning=*/false, /*forkIdDigest=*/false);
    const auto ok = ValidateAtHeights(se, original, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/true);
    BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(ok),
        "original digest, pre-UAHF consensus: domain=" << ok.domain << " code=" << ok.code);

    const auto policy = ValidateAtHeights(se, original, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/false);
    BOOST_CHECK_MESSAGE(policy.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT)
        && policy.code == static_cast<int32_t>(SCRIPT_ERR_MUST_USE_FORKID),
        "original digest, pre-UAHF policy: domain=" << policy.domain << " code=" << policy.code);

    const auto bip143 = MakeSignedP2PKHSpend(types, /*nOutputs=*/types.size());
    const auto bad = ValidateAtHeights(se, bip143, PRE_UAHF_UTXO_HEIGHT, PRE_UAHF_BLOCK_HEIGHT, /*consensus=*/true);
    BOOST_CHECK_MESSAGE(bad.domain == static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT)
        && bad.code == static_cast<int32_t>(SCRIPT_ERR_EVAL_FALSE),
        "BIP143 digest, pre-UAHF consensus: domain=" << bad.domain << " code=" << bad.code);
}

// Timing tests. Both are disabled by default and carry the `timing` label;
// ctest runs them as their own entry (`--run_test=@timing`, see
// test/core/CMakeLists.txt). They time CPU work in small samples, so they
// skip in a non-optimised build (no NDEBUG), where debug code changes the
// ratios they bound, and on Windows, whose process CPU clock (GetProcessTimes)
// ticks every ~15.6 ms, too coarse for samples of a few milliseconds.
static boost::test_tools::assertion_result OptimisedBuild(boost::unit_test::test_unit_id)
{
#if defined(_WIN32)
    boost::test_tools::assertion_result skip(false);
    skip.message() << "timing test skipped: Windows process CPU time is too coarse for these samples";
    return skip;
#elif !defined(NDEBUG)
    boost::test_tools::assertion_result skip(false);
    skip.message() << "timing test skipped: not an optimised (NDEBUG) build";
    return skip;
#else
    return true;
#endif
}

// CPU time of the whole process, every thread, user plus kernel, in seconds.
// Process, not thread, time: work moved to helper threads must still count.
// Wall-clock would charge a test for time spent descheduled on a busy runner.
// Not implemented on Windows: the timing tests skip there (see above), and no
// CI job runs on Windows today.
static double ProcessCpuSeconds()
{
#ifdef _WIN32
    BOOST_FAIL("ProcessCpuSeconds() is not implemented on Windows; the timing tests skip there");
    return 0.0;
#else
    timespec now{};
    BOOST_REQUIRE(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now) == 0);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
#endif
}

// An N-input tx whose input `index` spends a bare K-of-K OP_CHECKMULTISIG,
// signed ALL|FORKID by K distinct keys in pubkey order. VerifySpend checks only
// that input, so the other scriptSigs stay empty. The tx has as many outputs
// as inputs, so re-hashing any one of the three aggregates per check shows up.
// Returns the plain tx bytes and the spent output.
struct ManyKeyMultisigSpend {
    std::vector<uint8_t> txBin;
    CTxOut source;
    uint32_t index;
};

static ManyKeyMultisigSpend MakeManyKeyMultisigSpend(size_t nInputs, size_t nKeys)
{
    const SigHashType allForkId = SigHashType().withForkId();
    const uint32_t index = static_cast<uint32_t>(nInputs / 2);

    std::vector<CKey> keys;
    CScript multisig = CScript() << static_cast<int64_t>(nKeys);
    for (size_t k = 0; k < nKeys; ++k) {
        keys.push_back(MakeDeterministicTestKey(static_cast<uint8_t>(k & 0xff), static_cast<uint8_t>(1 + (k >> 8))));
        multisig << ToByteVector(keys.back().GetPubKey());
    }
    multisig << static_cast<int64_t>(nKeys) << OP_CHECKMULTISIG;

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nLockTime = 0;
    mtx.vin.resize(nInputs);
    for (size_t i = 0; i < nInputs; ++i) {
        mtx.vin[i].prevout = COutPoint(uint256S("01"), static_cast<uint32_t>(i));
        mtx.vin[i].nSequence = 0xffffffff - static_cast<uint32_t>(i % 3);
    }
    const CScript payTo = MakeP2PKHScript(MakeP2PKHTestKey().GetPubKey());
    mtx.vout.resize(nInputs);
    for (size_t j = 0; j < nInputs; ++j) {
        mtx.vout[j].nValue = Amount(P2PKH_OUTPUT_SATOSHIS + static_cast<int64_t>(j));
        mtx.vout[j].scriptPubKey = payTo;
    }

    const CTxOut source(Amount(P2PKH_INPUT_SATOSHIS), multisig);
    const CTransaction unsignedTx(mtx);
    const PrecomputedTransactionData txdata(unsignedTx);
    const uint256 sighash = SignatureHash(multisig, unsignedTx, index, allForkId, source.nValue, &txdata);
    CScript scriptSig = CScript() << OP_0;
    for (const CKey& key : keys) {
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(key.Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(allForkId.getRawSigHashType()));
        scriptSig << sig;
    }
    mtx.vin[index].scriptSig = scriptSig;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << mtx;
    return { std::vector<uint8_t>(ss.begin(), ss.end()), source, index };
}

// VerifySpend must share one PrecomputedTransactionData across the signature
// checks of its input. Without it each of K distinct FORKID checks re-hashes
// every outpoint, sequence and output of the tx, so one input costs K x O(N).
//
// The test measures the marginal cost of one extra signature check,
// (t(K=129) - t(K=1)) / 128, on a 2,000-input and a 60,000-input tx, and bounds
// the ratio of the two. With the shared precompute an extra check is one ECDSA
// verify whatever the tx size, so the ratio stays near 1; re-hashing per check
// makes it grow with N. Subtracting the K=1 time takes out parsing and the
// precompute, whose cost depends on how fast SHA-256 runs. Each of the four
// timings is the best of 7 samples of 5 calls, the samples interleaved.
BOOST_AUTO_TEST_CASE(verify_spend_shares_sighash_precompute_across_checks,
    * boost::unit_test::label("timing") * boost::unit_test::disabled()
    * boost::unit_test::precondition(OptimisedBuild))
{
    constexpr std::array<size_t, 2> N_INPUTS{ 2000, 60000 };
    constexpr std::array<size_t, 2> N_KEYS{ 1, 129 };
    constexpr int SAMPLES = 7;
    constexpr int CALLS_PER_SAMPLE = 5;
    constexpr double MAX_RATIO = 3.0;

    const bsv::CTxValidator se("main");

    // spends[n][k], bestMs[n][k] for N_INPUTS[n], N_KEYS[k]
    std::array<std::array<ManyKeyMultisigSpend, 2>, 2> spends;
    std::array<std::array<double, 2>, 2> bestMs;
    for (size_t n = 0; n < 2; ++n) {
        for (size_t k = 0; k < 2; ++k) {
            spends[n][k] = MakeManyKeyMultisigSpend(N_INPUTS[n], N_KEYS[k]);
            bestMs[n][k] = std::numeric_limits<double>::max();
        }
    }

    for (int sample = 0; sample < SAMPLES; ++sample) {
        for (size_t n = 0; n < 2; ++n) {
            for (size_t k = 0; k < 2; ++k) {
                const ManyKeyMultisigSpend& spend = spends[n][k];
                const std::span<const uint8_t> lockingScript(
                    spend.source.scriptPubKey.data(), spend.source.scriptPubKey.size());
                const double t0 = ProcessCpuSeconds();
                for (int call = 0; call < CALLS_PER_SAMPLE; ++call) {
                    const auto status = se.VerifySpend(std::span<const uint8_t>(spend.txBin), spend.index,
                        lockingScript, spend.source.nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT,
                        /*consensus=*/true);
                    BOOST_REQUIRE_MESSAGE(bsv::TxErrorIsOk(status), "N=" << N_INPUTS[n] << " K=" << N_KEYS[k]
                        << ": domain=" << status.domain << " code=" << status.code);
                }
                const double ms = 1000.0 * (ProcessCpuSeconds() - t0) / CALLS_PER_SAMPLE;
                bestMs[n][k] = std::min(bestMs[n][k], ms);
            }
        }
    }

    const double extraChecks = static_cast<double>(N_KEYS[1] - N_KEYS[0]);
    const double smallMarginalMs = (bestMs[0][1] - bestMs[0][0]) / extraChecks;
    const double largeMarginalMs = (bestMs[1][1] - bestMs[1][0]) / extraChecks;
    BOOST_REQUIRE_MESSAGE(smallMarginalMs > 0.0,
        "N=" << N_INPUTS[0] << ": " << N_KEYS[1] << " checks measured no slower than " << N_KEYS[0]
        << " (best_cpu_ms " << bestMs[0][1] << " vs " << bestMs[0][0] << ")");
    BOOST_REQUIRE_MESSAGE(largeMarginalMs > 0.0,
        "N=" << N_INPUTS[1] << ": " << N_KEYS[1] << " checks measured no slower than " << N_KEYS[0]
        << " (best_cpu_ms " << bestMs[1][1] << " vs " << bestMs[1][0] << ")");
    const double ratio = largeMarginalMs / smallMarginalMs;

    std::cerr << "[verify_spend_shares_sighash_precompute_across_checks]";
    for (size_t n = 0; n < 2; ++n)
        for (size_t k = 0; k < 2; ++k)
            std::cerr << " N=" << N_INPUTS[n] << ",K=" << N_KEYS[k] << " best_cpu_ms=" << bestMs[n][k];
    std::cerr << " marginal_ms N=" << N_INPUTS[0] << ":" << smallMarginalMs << " N=" << N_INPUTS[1] << ":"
              << largeMarginalMs << " ratio=" << ratio << " max_ratio=" << MAX_RATIO << std::endl;

    BOOST_CHECK_MESSAGE(ratio < MAX_RATIO,
        "the CPU cost of one extra VerifySpend signature check grew " << ratio << "x from " << N_INPUTS[0]
        << " to " << N_INPUTS[1] << " inputs, over " << MAX_RATIO << ": each check is re-hashing the whole tx");
}

// Script verification must share one PrecomputedTransactionData across the
// inputs of a tx, as SV Node does. Without it every FORKID signature check
// re-hashes all outpoints, sequences and outputs, so ValidateTransaction grows
// with the square of the input count: mainnet tx
// 4201a692b7349e7d047274029317460449b1a30f734eb43991c44bf15fa4382a (86,584
// inputs) took about 26 minutes.
//
// The test times the same P2PKH shape at two sizes and bounds the ratio, which
// cancels out the runner's absolute speed. Inputs and outputs grow together,
// so re-hashing any one of the three aggregates per input shows up. It
// measures process CPU time (ProcessCpuSeconds above). Best of 3 per size
// absorbs the remaining noise (cache and frequency effects).
BOOST_AUTO_TEST_CASE(validate_transaction_scales_linearly_with_input_count,
    * boost::unit_test::label("timing") * boost::unit_test::disabled()
    * boost::unit_test::precondition(OptimisedBuild))
{
    constexpr size_t SMALL_N = 1000;
    constexpr size_t LARGE_N = 16000;
    constexpr int RUNS = 3;
    constexpr double MAX_RATIO = 40.0;

    const bsv::CTxValidator se("main");
    const SigHashType allForkId = SigHashType().withForkId();

    const auto bestTimeMs = [&](size_t nInputs) {
        const std::vector<SigHashType> types(nInputs, allForkId);
        const auto etxBin = SerializeExtended(
            MakeSignedP2PKHSpend(types, /*nOutputs=*/nInputs, /*precomputeForSigning=*/true));
        double best = std::numeric_limits<double>::max();
        for (int run = 0; run < RUNS; ++run) {
            const double t0 = ProcessCpuSeconds();
            const auto status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
            const double elapsed = 1000.0 * (ProcessCpuSeconds() - t0);
            BOOST_REQUIRE_MESSAGE(bsv::TxErrorIsOk(status),
                "N=" << nInputs << ": domain=" << status.domain << " code=" << status.code);
            best = std::min(best, elapsed);
        }
        return best;
    };

    const double smallMs = bestTimeMs(SMALL_N);
    const double largeMs = bestTimeMs(LARGE_N);
    const double ratio = largeMs / smallMs;

    // stderr so ctest --output-on-failure shows the measurement on regression.
    std::cerr << "[validate_transaction_scales_linearly_with_input_count]"
              << " N=" << SMALL_N << " best_cpu_ms=" << smallMs
              << " N=" << LARGE_N << " best_cpu_ms=" << largeMs
              << " ratio=" << ratio << " max_ratio=" << MAX_RATIO << std::endl;

    BOOST_CHECK_MESSAGE(ratio < MAX_RATIO,
        "ValidateTransaction CPU time ratio " << ratio << " for " << LARGE_N / SMALL_N
        << "x the inputs exceeds " << MAX_RATIO << ": sighash work is growing with the square of the input count");
}

// Sets the parallel input threshold for one test and restores it after.
struct InputThresholdGuard {
    explicit InputThresholdGuard(size_t minInputs)
        : previous(bsv::detail::SetParallelInputThresholdForTesting(minInputs)) {}
    ~InputThresholdGuard() { bsv::detail::SetParallelInputThresholdForTesting(previous); }
    InputThresholdGuard(const InputThresholdGuard&) = delete;
    InputThresholdGuard& operator=(const InputThresholdGuard&) = delete;
    const size_t previous;
};

constexpr size_t FORCE_PARALLEL = 2;
constexpr size_t FORCE_SERIAL = std::numeric_limits<size_t>::max();

// Script verification of a tx with many inputs runs on several threads. These
// tests pin that the parallel path returns exactly what the serial path does.
// A lost wakeup would hang a test. Each case has a timeout that turns a hang
// into a failure (Boost.Test raises SIGALRM; ctest's TIMEOUT backs it up), and
// every case after the first depends on it: a job that never closes hangs the
// first case, which fails at the timeout, and the rest are skipped instead of
// each hanging for its own timeout.
BOOST_AUTO_TEST_SUITE(parallel_input_verification)

constexpr unsigned PARALLEL_TEST_TIMEOUT_S = 60;
#define PARALLEL_TEST_CASE_DECORATORS \
    *boost::unit_test::timeout(PARALLEL_TEST_TIMEOUT_S) \
    *boost::unit_test::depends_on("test_txvalidator/parallel_input_verification/valid_tx_validates_with_inputs_spread_over_helpers")

// The library ships with parallel verification on, from the default threshold.
BOOST_AUTO_TEST_CASE(shipped_threshold_is_the_default)
{
    BOOST_CHECK_EQUAL(shippedParallelInputThreshold, bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
}

// The library ships with the test-only input counters off, so production
// does no per-input write to them.
BOOST_AUTO_TEST_CASE(input_counters_are_off_by_default)
{
    BOOST_CHECK(!shippedInputCountersEnabled);
}

BOOST_AUTO_TEST_CASE(valid_tx_validates_with_inputs_spread_over_helpers, *boost::unit_test::timeout(PARALLEL_TEST_TIMEOUT_S))
{
    constexpr size_t nInputs = 200;
    const bsv::CTxValidator se("main");
    const auto etxBin = SerializeExtended(MakeSignedP2PKHSpend(
        std::vector<SigHashType>(nInputs, SigHashType().withForkId()), /*nOutputs=*/2, /*precomputeForSigning=*/true));

    const InputThresholdGuard parallel(FORCE_PARALLEL);
    const uint64_t helperInputsBefore = bsv::detail::HelperVerifiedInputCountForTesting();
    for (const bool consensus : { true, false }) {
        const auto status = ValidateP2PKHSpend(se, etxBin, nInputs, consensus);
        BOOST_CHECK_MESSAGE(bsv::TxErrorIsOk(status),
            "consensus=" << consensus << ": domain=" << status.domain << " code=" << status.code);
    }
    // A loaded machine may not schedule a helper within one short call; a few
    // more calls give one the chance without making the test slow.
    for (int retry = 0; retry < 50 && bsv::detail::HelperVerifiedInputCountForTesting() == helperInputsBefore; ++retry)
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
    BOOST_CHECK_MESSAGE(bsv::detail::HelperVerifiedInputCountForTesting() > helperInputsBefore,
        "no input was verified on a helper thread");
}

// The serial reference: VerifySpend on each input in order, the first failure wins.
static TxError FirstFailingSpend(const bsv::CTxValidator& se,
                                 const bsv::CMutableTransactionExtended& eTx,
                                 const std::vector<int32_t>& utxoHeights,
                                 bool consensus)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx.mtx;
    const std::vector<uint8_t> txBin(ss.begin(), ss.end());
    for (size_t i = 0; i < eTx.vutxo.size(); ++i) {
        const CTxOut& source = eTx.vutxo[i];
        const TxError status = se.VerifySpend(std::span<const uint8_t>(txBin), static_cast<uint32_t>(i),
            std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
            source.nValue.GetSatoshis(), utxoHeights[i], P2PKH_BLOCK_HEIGHT, consensus);
        if (!bsv::TxErrorIsOk(status))
            return status;
    }
    return bsv::TxErrorOk();
}

static bool SameTxError(const TxError& a, const TxError& b)
{
    return a.domain == b.domain && a.code == b.code;
}

// ValidateTransaction forced parallel, forced serial, and the VerifySpend
// reference all return `expected`, in both modes.
static void CheckParallelMatchesSerial(const bsv::CTxValidator& se,
                                       const bsv::CMutableTransactionExtended& eTx,
                                       const std::vector<int32_t>& utxoHeights,
                                       bool consensus,
                                       const TxError& expected,
                                       const std::string& label)
{
    const auto etxBin = SerializeExtended(eTx);
    const auto validate = [&](size_t threshold) {
        const InputThresholdGuard guard(threshold);
        return se.ValidateTransaction(std::span<const uint8_t>(etxBin),
            std::span<const int32_t>(utxoHeights), P2PKH_BLOCK_HEIGHT, consensus);
    };
    const TxError parallel = validate(FORCE_PARALLEL);
    const TxError serial = validate(FORCE_SERIAL);
    const TxError reference = FirstFailingSpend(se, eTx, utxoHeights, consensus);
    BOOST_CHECK_MESSAGE(SameTxError(reference, expected),
        label << " consensus=" << consensus << ": reference " << reference.domain << "/" << reference.code
        << " expected " << expected.domain << "/" << expected.code);
    BOOST_CHECK_MESSAGE(SameTxError(serial, expected),
        label << " consensus=" << consensus << ": serial " << serial.domain << "/" << serial.code
        << " expected " << expected.domain << "/" << expected.code);
    BOOST_CHECK_MESSAGE(SameTxError(parallel, expected),
        label << " consensus=" << consensus << ": parallel " << parallel.domain << "/" << parallel.code
        << " expected " << expected.domain << "/" << expected.code);
}

// Replaces input `index`'s signature with an empty push: OP_CHECKSIG pushes
// false without tripping NULLFAIL, so the input fails with EVAL_FALSE.
static void EmptyP2PKHSignature(bsv::CMutableTransactionExtended& eTx, size_t index)
{
    CScript::const_iterator pc = eTx.mtx.vin[index].scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> sig;
    std::vector<uint8_t> pubkey;
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, sig));
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, pubkey));
    eTx.mtx.vin[index].scriptSig = CScript() << std::vector<uint8_t>() << pubkey;
}

constexpr size_t PARITY_INPUTS = 120;

static bsv::CMutableTransactionExtended MakeParitySpend()
{
    return MakeSignedP2PKHSpend(std::vector<SigHashType>(PARITY_INPUTS, SigHashType().withForkId()),
        /*nOutputs=*/2, /*precomputeForSigning=*/true);
}

BOOST_AUTO_TEST_CASE(bad_signature_at_first_middle_and_last_input, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    const auto base = MakeParitySpend();
    for (const size_t k : { size_t{ 0 }, PARITY_INPUTS / 2, PARITY_INPUTS - 1 }) {
        auto eTx = base;
        CorruptP2PKHSignature(eTx, k);
        for (const bool consensus : { true, false })
            CheckParallelMatchesSerial(se, eTx, utxoHeights, consensus,
                bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL), "k=" + std::to_string(k));
    }
}

// Two failing inputs with different errors: the lower index's error is the
// result, whichever of them a thread reaches first.
BOOST_AUTO_TEST_CASE(lowest_failing_input_wins, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    struct Case { size_t evalFalseAt; size_t nullFailAt; TxError expected; };
    const std::array<Case, 3> cases{ {
        { 10, 90, bsv::TxErrorScript(SCRIPT_ERR_EVAL_FALSE) },
        { 90, 10, bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL) },
        { PARITY_INPUTS - 1, PARITY_INPUTS - 2, bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL) },
    } };
    for (const Case& c : cases) {
        auto eTx = MakeParitySpend();
        EmptyP2PKHSignature(eTx, c.evalFalseAt);
        CorruptP2PKHSignature(eTx, c.nullFailAt);
        for (const bool consensus : { true, false })
            CheckParallelMatchesSerial(se, eTx, utxoHeights, consensus, c.expected,
                "evalFalseAt=" + std::to_string(c.evalFalseAt) + " nullFailAt=" + std::to_string(c.nullFailAt));
    }
}

// A consensus-mode input with no confirmed height fails with
// UnconfirmedInputInBlock at its index; a lower bad signature still wins.
// In policy mode the unconfirmed input is valid and the signature decides.
BOOST_AUTO_TEST_CASE(unconfirmed_input_in_block_competes_by_index, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const TxError nullFail = bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL);
    const TxError unconfirmed = bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::UnconfirmedInputInBlock));
    struct Case { size_t mempoolAt; size_t badSigAt; TxError expectedInBlock; };
    for (const Case& c : { Case{ 30, 60, unconfirmed }, Case{ 60, 30, nullFail } }) {
        auto eTx = MakeParitySpend();
        CorruptP2PKHSignature(eTx, c.badSigAt);
        std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
        utxoHeights[c.mempoolAt] = MEMPOOL_HEIGHT;
        const std::string label = "mempoolAt=" + std::to_string(c.mempoolAt) + " badSigAt=" + std::to_string(c.badSigAt);
        CheckParallelMatchesSerial(se, eTx, utxoHeights, /*consensus=*/true, c.expectedInBlock, label);
        CheckParallelMatchesSerial(se, eTx, utxoHeights, /*consensus=*/false, nullFail, label);
    }
}

// An exception thrown while verifying inputs reaches the entry point's handler
// from a helper thread as it does from the serial loop. A block height whose
// next block is MEMPOOL_HEIGHT makes every input's flag calculation throw.
BOOST_AUTO_TEST_CASE(exception_while_verifying_inputs_is_reported, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const auto etxBin = SerializeExtended(MakeParitySpend());
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    for (const size_t threshold : { FORCE_PARALLEL, FORCE_SERIAL }) {
        const InputThresholdGuard guard(threshold);
        const TxError status = se.VerifyScript(std::span<const uint8_t>(etxBin),
            std::span<const int32_t>(utxoHeights), MEMPOOL_HEIGHT - 1, /*consensus=*/false);
        BOOST_CHECK_MESSAGE(SameTxError(status, bsv::TxErrorException()),
            "threshold=" << threshold << ": domain=" << status.domain << " code=" << status.code);
    }
}

// VerifyScript with per-input custom flags: input 70 loses SIGHASH_FORKID
// support, input 25 has a bad signature. Each failure alone, then both
// (lowest wins), against VerifySpend with the same flags.
BOOST_AUTO_TEST_CASE(custom_flags_per_input_through_verify_script, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    for (const bool consensus : { true, false }) {
        const uint32_t standard = se.CalculateFlags(P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT, consensus);
        std::vector<uint32_t> flags(PARITY_INPUTS, standard);
        flags[70] = standard & ~SCRIPT_ENABLE_SIGHASH_FORKID;

        for (const bool badSig : { false, true }) {
            auto eTx = MakeParitySpend();
            if (badSig)
                CorruptP2PKHSignature(eTx, 25);
            const auto etxBin = SerializeExtended(eTx);

            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << eTx.mtx;
            const std::vector<uint8_t> txBin(ss.begin(), ss.end());
            TxError reference = bsv::TxErrorOk();
            for (size_t i = 0; i < PARITY_INPUTS && bsv::TxErrorIsOk(reference); ++i) {
                const CTxOut& source = eTx.vutxo[i];
                reference = se.VerifySpend(std::span<const uint8_t>(txBin), static_cast<uint32_t>(i),
                    std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
                    source.nValue.GetSatoshis(), utxoHeights[i], P2PKH_BLOCK_HEIGHT, consensus, flags[i]);
            }
            const TxError expected = badSig ? bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL)
                                            : bsv::TxErrorScript(SCRIPT_ERR_ILLEGAL_FORKID);
            BOOST_CHECK_MESSAGE(SameTxError(reference, expected),
                "consensus=" << consensus << " badSig=" << badSig << ": reference " << reference.domain << "/" << reference.code);

            for (const size_t threshold : { FORCE_PARALLEL, FORCE_SERIAL }) {
                const InputThresholdGuard guard(threshold);
                const TxError status = se.VerifyScript(std::span<const uint8_t>(etxBin),
                    std::span<const int32_t>(utxoHeights), P2PKH_BLOCK_HEIGHT, consensus,
                    std::span<const uint32_t>(flags));
                BOOST_CHECK_MESSAGE(SameTxError(status, expected),
                    "consensus=" << consensus << " badSig=" << badSig << " threshold=" << threshold
                    << ": " << status.domain << "/" << status.code);
            }
        }
    }
}

// Mixed FORKID sighash types and a 2-of-3 multisig input on a large tx, valid
// and with the multisig's second signature broken.
BOOST_AUTO_TEST_CASE(mixed_sighash_types_and_multisig_input, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    std::vector<SigHashType> types;
    for (size_t i = 0; i < PARITY_INPUTS; ++i)
        types.push_back(FORKID_SIGHASH_TYPES[i % FORKID_SIGHASH_TYPES.size()]);
    const auto mixed = MakeSignedP2PKHSpend(types, /*nOutputs=*/PARITY_INPUTS, /*precomputeForSigning=*/true);
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);

    MultisigSpend multisig = MakeSpendWithMultisigInput(PARITY_INPUTS, /*index=*/PARITY_INPUTS / 3, /*nOutputs=*/3);
    MultisigSpend brokenMultisig = multisig;
    CorruptSecondMultisigSignature(brokenMultisig);

    for (const bool consensus : { true, false }) {
        CheckParallelMatchesSerial(se, mixed, utxoHeights, consensus, bsv::TxErrorOk(), "mixed sighash");
        CheckParallelMatchesSerial(se, multisig.eTx, utxoHeights, consensus, bsv::TxErrorOk(), "multisig");
        CheckParallelMatchesSerial(se, brokenMultisig.eTx, utxoHeights, consensus,
            bsv::TxErrorScript(SCRIPT_ERR_SIG_NULLFAIL), "broken multisig");
    }
}

// ValidateBatch with small and large txs, good and bad: each result is that
// tx's serial result.
BOOST_AUTO_TEST_CASE(validate_batch_results_match_serial, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const SigHashType allForkId = SigHashType().withForkId();
    std::vector<bsv::CMutableTransactionExtended> txs;
    txs.push_back(MakeSignedP2PKHSpend(std::vector<SigHashType>(3, allForkId), 2));
    txs.push_back(MakeParitySpend());
    txs.push_back(MakeParitySpend());
    CorruptP2PKHSignature(txs.back(), 77);
    txs.push_back(MakeSignedP2PKHSpend(std::vector<SigHashType>(3, allForkId), 2));
    EmptyP2PKHSignature(txs.back(), 1);
    txs.push_back(MakeParitySpend());
    EmptyP2PKHSignature(txs.back(), 5);
    CorruptP2PKHSignature(txs.back(), 4);

    std::vector<std::vector<uint8_t>> bins;
    std::vector<std::vector<int32_t>> heights;
    for (const auto& tx : txs) {
        bins.push_back(SerializeExtended(tx));
        heights.emplace_back(tx.vutxo.size(), P2PKH_UTXO_HEIGHT);
    }
    for (const bool consensus : { true, false }) {
        bsv::ValidateBatch batch;
        for (size_t i = 0; i < txs.size(); ++i)
            batch.add(bsv::ValidateArg(bins[i], heights[i], P2PKH_BLOCK_HEIGHT, consensus));
        std::vector<TxError> parallel;
        {
            const InputThresholdGuard guard(FORCE_PARALLEL);
            parallel = se.ValidateBatch(batch);
        }
        BOOST_REQUIRE_EQUAL(parallel.size(), txs.size());
        for (size_t i = 0; i < txs.size(); ++i) {
            const TxError reference = FirstFailingSpend(se, txs[i], heights[i], consensus);
            BOOST_CHECK_MESSAGE(SameTxError(parallel[i], reference),
                "tx " << i << " consensus=" << consensus << ": batch " << parallel[i].domain << "/" << parallel[i].code
                << " reference " << reference.domain << "/" << reference.code);
        }
    }
}

// Rewrites input `index`'s scriptSig with `edit` applied to its signature and
// public key pushes.
template <typename Edit>
static void EditP2PKHScriptSig(bsv::CMutableTransactionExtended& eTx, size_t index, Edit edit)
{
    CScript::const_iterator pc = eTx.mtx.vin[index].scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> sig;
    std::vector<uint8_t> pubkey;
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, sig));
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, pubkey));
    edit(sig, pubkey);
    eTx.mtx.vin[index].scriptSig = CScript() << sig << pubkey;
}

// Many callers validate different large txs at once, each failing tx with its
// own error, while sharing the pool: every result equals that tx's serial
// result. Fewer callers than cores leave helpers free, so a good share of the
// inputs runs on helpers and a result leaking from one tx's job into
// another's shows up as a wrong error.
BOOST_AUTO_TEST_CASE(concurrent_callers_share_the_pool, PARALLEL_TEST_CASE_DECORATORS)
{
    constexpr size_t ROUNDS = 24;
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    const size_t callerCount = std::max<size_t>(2, (helpers + 1) / 2);
    const bsv::CTxValidator se("main");
    const SigHashType allForkId = SigHashType().withForkId();

    // Failing txs are short with early failures, so jobs fail often and close
    // together; each fails with its own error.
    constexpr size_t FAILING_INPUTS = 40;
    const auto failing = [&] {
        return MakeSignedP2PKHSpend(std::vector<SigHashType>(FAILING_INPUTS, allForkId), 2, /*precomputeForSigning=*/true);
    };
    std::vector<bsv::CMutableTransactionExtended> txs;
    txs.push_back(MakeParitySpend());                                          // valid
    txs.push_back(MakeSignedP2PKHSpend(std::vector<SigHashType>(3, allForkId), 2));  // small, valid
    txs.push_back(failing());
    CorruptP2PKHSignature(txs.back(), 7);                                      // NULLFAIL
    txs.push_back(failing());
    EmptyP2PKHSignature(txs.back(), 12);                                       // EVAL_FALSE, above it NULLFAIL
    CorruptP2PKHSignature(txs.back(), 30);
    txs.push_back(failing());
    EditP2PKHScriptSig(txs.back(), 20, [](auto&, auto& pubkey) {               // EQUALVERIFY
        pubkey = ToByteVector(MakeDeterministicTestKey(0x70).GetPubKey());
    });
    txs.push_back(failing());
    EditP2PKHScriptSig(txs.back(), 3, [](auto& sig, auto&) { sig[0] = 0x31; });  // SIG_DER, above it NULLFAIL
    CorruptP2PKHSignature(txs.back(), 4);
    {
        std::vector<SigHashType> types(FAILING_INPUTS, allForkId);
        types[25] = SigHashType();                                             // MUST_USE_FORKID
        txs.push_back(MakeSignedP2PKHSpend(types, 2, /*precomputeForSigning=*/true));
    }
    txs.push_back(failing());                                                  // UnconfirmedInputInBlock in a block

    std::vector<std::vector<uint8_t>> bins;
    std::vector<std::vector<int32_t>> heights;
    std::vector<std::array<TxError, 2>> expected;  // [consensus]
    for (const auto& tx : txs) {
        bins.push_back(SerializeExtended(tx));
        heights.emplace_back(tx.vutxo.size(), P2PKH_UTXO_HEIGHT);
    }
    heights.back()[5] = MEMPOOL_HEIGHT;
    for (size_t i = 0; i < txs.size(); ++i)
        expected.push_back({ FirstFailingSpend(se, txs[i], heights[i], false),
                             FirstFailingSpend(se, txs[i], heights[i], true) });

    // Every failing tx fails differently, in each mode.
    for (const bool consensus : { false, true }) {
        for (size_t i = 0; i < txs.size(); ++i)
            for (size_t j = i + 1; j < txs.size(); ++j)
                if (!bsv::TxErrorIsOk(expected[i][consensus]))
                    BOOST_REQUIRE_MESSAGE(!SameTxError(expected[i][consensus], expected[j][consensus]),
                        "txs " << i << " and " << j << " expect the same error, consensus=" << consensus);
    }

    const InputThresholdGuard parallel(FORCE_PARALLEL);
    std::atomic<size_t> mismatches{ 0 };
    std::atomic<uint64_t> inputs{ 0 };
    const uint64_t helperInputsBefore = bsv::detail::HelperVerifiedInputCountForTesting();
    std::vector<std::thread> callers;
    // Two failing jobs mix results only if one fails while the other is between
    // its own failure and its return, a window of about one input; enough
    // rounds make that overlap happen at CI core counts.
    const auto stop = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    for (size_t t = 0; t < callerCount; ++t) {
        callers.emplace_back([&, t] {
            for (size_t r = 0; r < ROUNDS || std::chrono::steady_clock::now() < stop; ++r) {
                const size_t i = (t * 3 + r) % txs.size();
                const bool consensus = ((t + r / txs.size()) & 1) != 0;
                const TxError status = se.ValidateTransaction(std::span<const uint8_t>(bins[i]),
                    std::span<const int32_t>(heights[i]), P2PKH_BLOCK_HEIGHT, consensus);
                inputs.fetch_add(heights[i].size());
                if (!SameTxError(status, expected[i][consensus]))
                    mismatches.fetch_add(1);
            }
        });
    }
    for (auto& caller : callers)
        caller.join();
    BOOST_CHECK_EQUAL(mismatches.load(), 0u);

    const uint64_t helperInputs = bsv::detail::HelperVerifiedInputCountForTesting() - helperInputsBefore;
    BOOST_TEST_MESSAGE("callers=" << callerCount << " helpers=" << helpers << " inputs=" << inputs.load()
        << " on_helpers=" << helperInputs);
    // With a core left over for helpers (3 or more cores), at least a tenth of
    // the inputs run on them; measured 27-30% with the pool at 4 cores.
    if (callerCount < helpers + 1)
        BOOST_CHECK_MESSAGE(helperInputs * 10 >= inputs.load(),
            "only " << helperInputs << " of " << inputs.load() << " inputs ran on helpers");
    else
        BOOST_TEST_MESSAGE(helpers << " helper(s): the callers fill the pool, so jobs rarely overlap and results "
            "leaking between jobs could go unseen here");
}

// The pool's own behaviour, beyond results: when it is used, that the core
// budget returns to idle, that helpers leave an oversubscribed machine and come
// back, and that participants stop above a failure. These need a pool with at
// least one helper, i.e. a machine with 2 or more CPUs.
static bool PoolHasHelpers()
{
    if (bsv::detail::InputPoolHelperCountForTesting() > 0)
        return true;
    BOOST_TEST_MESSAGE("no input pool helpers on this machine; nothing to check");
    return false;
}

// Polls until `done()` holds or `deadline` passes; returns done().
template <typename Done>
static bool WaitFor(Done done, std::chrono::milliseconds deadline = std::chrono::milliseconds(5000))
{
    const auto stop = std::chrono::steady_clock::now() + deadline;
    while (!done() && std::chrono::steady_clock::now() < stop)
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    return done();
}

static bool PoolIsIdle()
{
    return WaitFor([] { return bsv::detail::BusyThreadCountForTesting() == 0; });
}

static std::vector<uint8_t> SignedP2PKHBin(size_t nInputs)
{
    return SerializeExtended(MakeSignedP2PKHSpend(std::vector<SigHashType>(nInputs, SigHashType().withForkId()),
        /*nOutputs=*/1, /*precomputeForSigning=*/true));
}

// A real thread-CPU source, recording every cross-thread read as well.
class CallerThreadBudget final : public task::CCancellationSource {
public:
    CallerThreadBudget()
        : owner{ std::this_thread::get_id() },
          timed{ task::CThreadTimedCancellationSource::Make(std::chrono::milliseconds(200))->GetToken() } {}
    bool IsCanceled() override
    {
        if (std::this_thread::get_id() != owner)
            ++otherThreadReads;
        return timed.IsCanceled();
    }
    std::atomic<size_t> otherThreadReads{ 0 };
private:
    const std::thread::id owner;
    const task::CCancellationToken timed;
};

class CpuBusyHelpers {
public:
    explicit CpuBusyHelpers(size_t helpers) : helpers{ helpers }
    {
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~CpuBusyHelpers() { bsv::detail::SetInputHookForTesting(previous); }
    std::atomic<size_t> burned{ 0 }, arrived{ 0 };
    std::atomic<bool> timedOut{ false };
    static inline thread_local bool isCaller = false;
private:
    static void Hook(size_t, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        auto& self = *active;
        if (isCaller) {
            if (!WaitFor([&] { return self.burned.load() == self.helpers; }, std::chrono::milliseconds(20000)))
                self.timedOut = true;
        } else if (!burnedOnThisJob) {
            burnedOnThisJob = true;
            ++self.arrived;
            if (!WaitFor([&] { return self.arrived.load() == self.helpers; }, std::chrono::milliseconds(20000)))
                self.timedOut = true;
            const auto start = task::thread_clock::now();
            while (task::thread_clock::now() - start < std::chrono::milliseconds(500))
                std::atomic_signal_fence(std::memory_order_seq_cst);
            ++self.burned;
        }
    }
    const size_t helpers;
    bsv::detail::InputHook previous;
    static inline CpuBusyHelpers* active = nullptr;
    static inline thread_local bool burnedOnThisJob = false;
};

BOOST_AUTO_TEST_CASE(thread_cpu_budget_is_polled_only_by_the_caller, PARALLEL_TEST_CASE_DECORATORS)
{
#ifndef BOOST_CHRONO_HAS_THREAD_CLOCK
    BOOST_TEST_MESSAGE("thread CPU clock unavailable");
#else
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0)
        return;
    const size_t nInputs = (helpers + 1) * 8;
    const auto bin = SignedP2PKHBin(nInputs);
    const std::vector<int32_t> heights(nInputs, P2PKH_UTXO_HEIGHT);
    const bsv::CTxValidator validator("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    CpuBusyHelpers load(helpers);
    TxError result = bsv::TxErrorException();
    std::shared_ptr<CallerThreadBudget> source;
    bool callerBudgetExpired = true;
    // A fresh caller starts near zero thread CPU time. Each helper burns more
    // than the caller's budget before its first interpreter poll on this job.
    std::thread caller([&] {
        CpuBusyHelpers::isCaller = true;
        source = std::make_shared<CallerThreadBudget>();
        result = validator.ValidateTransaction(bin, heights, P2PKH_BLOCK_HEIGHT, true, source->GetToken());
        callerBudgetExpired = source->IsCanceled();
    });
    caller.join();
    BOOST_CHECK(!load.timedOut);
    BOOST_CHECK_EQUAL(load.burned.load(), helpers);
    BOOST_CHECK_EQUAL(source->otherThreadReads.load(), 0u);
    BOOST_CHECK(!callerBudgetExpired);
    BOOST_CHECK(bsv::TxErrorIsOk(result));
    BOOST_CHECK(PoolIsIdle());
#endif
}

// Track real interpreter polls on inputs above the failing one. Hold the
// failing input until a higher input has reached its eighth poll, ensuring
// this exercises in-flight work rather than just unclaimed indices.
class EarlyStopProbe final : public task::CCancellationSource {
public:
    explicit EarlyStopProbe(size_t failingIndex, bool cancelOnFailure = false)
        : failingIndex{ failingIndex }, cancelOnFailure{ cancelOnFailure }
    {
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~EarlyStopProbe()
    {
        bsv::detail::SetInputHookForTesting(previous);
        active = nullptr;
    }

    bool IsCanceled() override { return CCancellationSource::IsCanceled(); }

    std::atomic<uint64_t> higherPolls{ 0 };
    std::atomic<size_t> entered{ 0 };
    std::atomic<bool> timedOut{ false };

private:
    static void Hook(size_t index, bsv::detail::InputEvent event)
    {
        auto& self = *active;
        if (event == bsv::detail::InputEvent::Claimed) {
            inputPolls = 0;
            if (index == self.failingIndex && !WaitFor([&] { return self.entered.load() > 0; }))
                self.timedOut = true;
        } else if (event == bsv::detail::InputEvent::Polled && index > self.failingIndex) {
            ++self.higherPolls;
            if (++inputPolls == 8) {
                ++self.entered;
                if (!WaitFor([&] { return self.failed.load(); }))
                    self.timedOut = true;
            }
        } else if (event == bsv::detail::InputEvent::Verified) {
            self.finished[index] = true;
        } else if (event == bsv::detail::InputEvent::Failed && index == self.failingIndex) {
            if (self.cancelOnFailure) {
                if (!WaitFor([&] {
                        for (size_t i = 0; i < index; ++i)
                            if (!self.finished[i]) return false;
                        return true;
                    }))
                    self.timedOut = true;
                self.Cancel();
            }
            self.failed = true;
        }
    }

    const size_t failingIndex;
    const bool cancelOnFailure;
    std::atomic<bool> failed{ false };
    std::array<std::atomic<bool>, 32> finished{};
    bsv::detail::InputHook previous;
    static inline EarlyStopProbe* active = nullptr;
    static inline thread_local uint64_t inputPolls = 0;
};

// A cheap failing input followed by many valid, expensive scripts. Lower
// inputs succeed cheaply; higher ones each hash 64 KiB 2,048 times.
static bsv::CMutableTransactionExtended EarlyStopSpend(size_t failingIndex)
{
    constexpr size_t inputs = 32;
    bsv::CMutableTransactionExtended tx;
    tx.mtx.nVersion = 2;
    tx.mtx.vout.emplace_back(Amount{ 1000 }, CScript() << OP_TRUE);
    for (size_t i = 0; i < inputs; ++i) {
        tx.mtx.vin.emplace_back(COutPoint(uint256S("01"), i),
            i > failingIndex ? CScript() << std::vector<uint8_t>(65536, 1) : CScript());
        CScript locking;
        if (i == failingIndex)
            locking << OP_FALSE;
        else if (i < failingIndex)
            locking << OP_TRUE;
        else
            for (size_t round = 0; round < 2048; ++round)
                locking << OP_DUP << OP_SHA256 << OP_DROP;
        tx.vutxo.emplace_back(Amount{ 100000 }, locking);
    }
    return tx;
}

BOOST_AUTO_TEST_CASE(early_failure_abandons_inflight_higher_inputs, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    const auto bin = SerializeExtended(EarlyStopSpend(0));
    const std::vector<int32_t> heights(32, 19000);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const auto probe = std::make_shared<EarlyStopProbe>(0);
    const auto result = validator.ValidateTransaction(bin, heights, 20000, true, probe->GetToken());
    BOOST_CHECK_EQUAL(result.domain, static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT));
    BOOST_CHECK_EQUAL(result.code, SCRIPT_ERR_EVAL_FALSE);
    BOOST_CHECK(!probe->timedOut);
    BOOST_REQUIRE_GT(probe->entered.load(), 0u);
    BOOST_TEST_MESSAGE("in-flight higher inputs=" << probe->entered.load()
        << " interpreter polls=" << probe->higherPolls.load());
    // Completing even one higher script takes over 6,000 polls. Early stop
    // should need only its initial polls, plus the polls until failure is seen.
    BOOST_CHECK_LT(probe->higherPolls.load(), 1000u);
}

// Pin the strict boundary on the actual source: a finished failure at index
// 5 must leave both index 5 and all lower inputs uncancelled. A <= mutation
// is otherwise invisible once that input has already finished verifying.
BOOST_AUTO_TEST_CASE(early_stop_source_preserves_equal_and_lower_indices)
{
    std::atomic<size_t> lowest{ 32 };
    const auto caller = task::CCancellationSource::Make();
    const auto callerToken = caller->GetToken();
    std::atomic<bool> cancelled{ false };
    const auto source = std::make_shared<bsv::detail::InputCancellationSource>(lowest, cancelled, &callerToken);
    const auto token = source->GetToken();
    source->SetIndex(5);
    BOOST_CHECK(!token.IsCanceled());
    lowest = 5;
    BOOST_CHECK(!token.IsCanceled());
    source->SetIndex(4);
    BOOST_CHECK(!token.IsCanceled());
    source->SetIndex(6);
    BOOST_CHECK(token.IsCanceled());
    lowest = 3;
    source->SetIndex(4);
    BOOST_CHECK(token.IsCanceled());
    source->SetIndex(2);
    BOOST_CHECK(!token.IsCanceled());
    caller->Cancel();
    BOOST_CHECK(token.IsCanceled());
}

// A completed failure and all lower inputs establish the serial verdict, even
// when cancellation stops still-running higher inputs.
BOOST_AUTO_TEST_CASE(completed_early_failure_survives_caller_cancellation, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    const std::vector<int32_t> heights(32, 19000);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    for (const size_t failingIndex : { 0, 1, 2, 8 }) {
        BOOST_REQUIRE(PoolIsIdle());
        const auto bin = SerializeExtended(EarlyStopSpend(failingIndex));
        const auto probe = std::make_shared<EarlyStopProbe>(failingIndex, true);
        const auto result = validator.ValidateTransaction(bin, heights, 20000, true, probe->GetToken());
        BOOST_CHECK(!probe->timedOut);
        BOOST_CHECK_GT(probe->entered.load(), 0u);
        BOOST_CHECK_EQUAL(result.domain, static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT));
        BOOST_CHECK_EQUAL(result.code, SCRIPT_ERR_EVAL_FALSE);
    }
}

BOOST_AUTO_TEST_CASE(early_stop_keeps_serial_verdict_for_mixed_failures, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    const std::vector<int32_t> heights(32, 19000);
    for (const size_t failingIndex : { 0, 1, 2, 8 }) {
        auto tx = EarlyStopSpend(failingIndex);
        // A distinct later verdict must neither win nor cancel lower indices.
        tx.vutxo[31].scriptPubKey = CScript() << OP_FALSE << OP_VERIFY;
        CDataStream plain(SER_NETWORK, PROTOCOL_VERSION);
        plain << tx.mtx;
        const std::vector<uint8_t> txBin(plain.begin(), plain.end());
        const auto& laterScript = tx.vutxo[31].scriptPubKey;
        const auto later = validator.VerifySpend(txBin, 31,
            std::span<const uint8_t>(laterScript.data(), laterScript.size()),
            100000, 19000, 20000, true);
        BOOST_REQUIRE_EQUAL(later.domain, static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT));
        BOOST_REQUIRE_EQUAL(later.code, SCRIPT_ERR_VERIFY);
        const auto bin = SerializeExtended(tx);
        TxError serial;
        {
            const InputThresholdGuard guard(FORCE_SERIAL);
            serial = validator.ValidateTransaction(bin, heights, 20000, true);
        }
        BOOST_REQUIRE_EQUAL(serial.domain, static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT));
        BOOST_REQUIRE_EQUAL(serial.code, SCRIPT_ERR_EVAL_FALSE);
        const InputThresholdGuard parallel(FORCE_PARALLEL);
        BOOST_REQUIRE(PoolIsIdle());
        const auto probe = std::make_shared<EarlyStopProbe>(failingIndex);
        const auto token = probe->GetToken();
        const auto result = validator.ValidateTransaction(bin, heights, 20000, true, token);
        BOOST_CHECK(!probe->timedOut);
        BOOST_CHECK_GT(probe->entered.load(), 0u);
        BOOST_CHECK(SameTxError(result, serial));
        BOOST_CHECK_LT(probe->higherPolls.load(), 1000u);
    }
}

// Cancel from an interpreter poll after a helper has joined a valid job.
// This catches dropping the caller token when constructing the input token.
class CancelDuringInput final : public task::CCancellationSource {
public:
    CancelDuringInput() : owner{ std::this_thread::get_id() }
    {
        inJob = false;
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~CancelDuringInput() { bsv::detail::SetInputHookForTesting(previous); }
    bool IsCanceled() override
    {
        if (inJob && ++polls >= 20) {
            if (!WaitFor([&] { return helperPolls.load() >= 8; }))
                timedOut = true;
            Cancel();
        }
        return CCancellationSource::IsCanceled();
    }
    std::atomic<uint64_t> polls{ 0 }, helperPolls{ 0 };
    std::atomic<bool> published{ false }, timedOut{ false };
private:
    static void Hook(size_t, bsv::detail::InputEvent event)
    {
        auto& self = *active;
        if (event == bsv::detail::InputEvent::Claimed) {
            inJob = true;
            helperInputPolls = 0;
        }
        if (event == bsv::detail::InputEvent::CallerCancelled)
            self.published = true;
        if (event == bsv::detail::InputEvent::Polled && std::this_thread::get_id() != self.owner) {
            ++self.helperPolls;
            if (++helperInputPolls == 8 && !WaitFor([&] { return self.published.load(); }))
                self.timedOut = true;
        }
    }
    const std::thread::id owner;
    static inline CancelDuringInput* active = nullptr;
    static inline thread_local bool inJob = false;
    static inline thread_local size_t helperInputPolls = 0;
    bsv::detail::InputHook previous;
};

BOOST_AUTO_TEST_CASE(caller_cancel_during_parallel_input_returns_cancelled, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    auto tx = EarlyStopSpend(0);
    tx.vutxo[0].scriptPubKey = CScript() << OP_TRUE;
    const auto bin = SerializeExtended(tx);
    const std::vector<int32_t> heights(32, 19000);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const auto source = std::make_shared<CancelDuringInput>();
    const auto result = validator.ValidateTransaction(bin, heights, 20000, true, source->GetToken());
    BOOST_CHECK_GE(source->polls.load(), 20u);
    BOOST_CHECK(!source->timedOut);
    BOOST_CHECK(source->published);
    BOOST_CHECK_GE(source->helperPolls.load(), 8u);
    BOOST_CHECK_LT(source->helperPolls.load(), 1000u);
    BOOST_CHECK_EQUAL(result.domain, static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED));
}

// Cancel after the requested verdict is already established: after the last
// valid input, or after a bad input and every lower input have finished.
class CancelAfterVerdict final : public task::CCancellationSource {
public:
    explicit CancelAfterVerdict(size_t failingIndex) : failingIndex{ failingIndex }
    {
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~CancelAfterVerdict() { bsv::detail::SetInputHookForTesting(previous); }
    std::atomic<bool> timedOut{ false };
private:
    static void Hook(size_t index, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Verified)
            return;
        auto& self = *active;
        self.finished[index] = true;
        const size_t count = ++self.count;
        if (index == self.failingIndex) {
            if (!WaitFor([&] {
                    for (size_t i = 0; i < index; ++i)
                        if (!self.finished[i]) return false;
                    return true;
                }))
                self.timedOut = true;
            self.Cancel();
        } else if (self.failingIndex == self.finished.size() && count == self.finished.size()) {
            self.Cancel();
        }
    }
    const size_t failingIndex;
    std::array<std::atomic<bool>, 32> finished{};
    std::atomic<size_t> count{ 0 };
    bsv::detail::InputHook previous;
    static inline CancelAfterVerdict* active = nullptr;
};

BOOST_AUTO_TEST_CASE(completed_verdict_survives_cancellation_in_serial_and_parallel, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    const std::vector<int32_t> heights(32, 19000);
    for (const size_t bad : { size_t{32}, size_t{0}, size_t{1}, size_t{2}, size_t{8} }) {
        auto tx = EarlyStopSpend(0);
        for (size_t i = 0; i < 32; ++i) {
            tx.mtx.vin[i].scriptSig = CScript();
            tx.vutxo[i].scriptPubKey = i == bad ? CScript() << OP_FALSE : CScript() << OP_TRUE;
        }
        const auto bin = SerializeExtended(tx);
        TxError serial = bsv::TxErrorException();
        for (const size_t threshold : { FORCE_SERIAL, FORCE_PARALLEL }) {
            BOOST_REQUIRE(PoolIsIdle());
            const InputThresholdGuard guard(threshold);
            const auto source = std::make_shared<CancelAfterVerdict>(bad);
            const auto result = validator.VerifyScript(bin, heights, 20000, true, {}, source->GetToken());
            BOOST_CHECK(!source->timedOut);
            BOOST_CHECK(source->IsCanceled());
            if (threshold == FORCE_SERIAL) {
                serial = result;
                BOOST_REQUIRE_EQUAL(result.domain, bad == 32 ? TX_ERR_DOMAIN_OK : TX_ERR_DOMAIN_SCRIPT);
            } else {
                BOOST_CHECK_MESSAGE(SameTxError(result, serial), "bad=" << bad << " parallel="
                    << result.domain << "/" << result.code << " serial=" << serial.domain << "/" << serial.code);
            }
        }
    }
}

// A helper's higher failure does not establish a verdict when input zero
// stops without completing. The caller observes cancellation in its interpreter.
class CancelBelowFailure final : public task::CCancellationSource {
public:
    CancelBelowFailure() : owner{ std::this_thread::get_id() }
    {
        inJob = false;
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~CancelBelowFailure() { bsv::detail::SetInputHookForTesting(previous); }
    bool IsCanceled() override
    {
        if (inJob) {
            if (!WaitFor([&] { return failure.load(); }))
                timedOut = true;
            Cancel();
        }
        return CCancellationSource::IsCanceled();
    }
    std::atomic<bool> failure{ false }, published{ false }, timedOut{ false }, callerClaimed{ false };
private:
    static void Hook(size_t index, bsv::detail::InputEvent event)
    {
        auto& self = *active;
        const bool caller = std::this_thread::get_id() == self.owner;
        if (event == bsv::detail::InputEvent::Claimed && caller) {
            inJob = true;
            self.callerClaimed = true;
        }
        if (event == bsv::detail::InputEvent::Failed && index > 0) {
            // Keep the real failure unpublished until the caller owns input
            // work on which it can observe cancellation.
            if (!caller && !WaitFor([&] { return self.callerClaimed.load(); }))
                self.timedOut = true;
            self.failure = true;
        }
        if (event == bsv::detail::InputEvent::CallerCancelled)
            self.published = true;
        if (event == bsv::detail::InputEvent::Polled && index == 0 && !caller
            && !WaitFor([&] { return self.published.load(); }))
            self.timedOut = true;
    }
    const std::thread::id owner;
    bsv::detail::InputHook previous;
    static inline CancelBelowFailure* active = nullptr;
    static inline thread_local bool inJob = false;
};

BOOST_AUTO_TEST_CASE(cancelled_lower_input_prevents_a_higher_failure_verdict, PARALLEL_TEST_CASE_DECORATORS)
{
    if (bsv::detail::InputPoolHelperCountForTesting() < 2) {
        BOOST_TEST_MESSAGE("needs two helpers: one holds input zero while another fails");
        return;
    }
    auto tx = EarlyStopSpend(0);
    for (size_t i = 0; i < 32; ++i) {
        tx.mtx.vin[i].scriptSig = CScript();
        tx.vutxo[i].scriptPubKey = i == 0 ? CScript() << OP_TRUE : CScript() << OP_FALSE;
    }
    const auto bin = SerializeExtended(tx);
    const std::vector<int32_t> heights(32, 19000);
    const bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    const auto alreadyCancelled = task::CCancellationSource::Make();
    alreadyCancelled->Cancel();
    TxError serial;
    {
        const InputThresholdGuard guard(FORCE_SERIAL);
        serial = validator.VerifyScript(bin, heights, 20000, true, {}, alreadyCancelled->GetToken());
    }
    BOOST_REQUIRE_EQUAL(serial.domain, TX_ERR_DOMAIN_CANCELLED);
    BOOST_REQUIRE(PoolIsIdle());
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    const auto source = std::make_shared<CancelBelowFailure>();
    const auto result = validator.VerifyScript(bin, heights, 20000, true, {}, source->GetToken());
    BOOST_CHECK(!source->timedOut);
    BOOST_CHECK(source->failure);
    BOOST_CHECK(source->published);
    BOOST_CHECK(SameTxError(result, serial));
}

// Every input claimed inside a parallel job sleeps for `delay` before it is
// verified: jobs last long without using CPU, so on a small machine the test
// thread still gets to run while the caller and helpers are on a job.
class SlowInputs {
public:
    explicit SlowInputs(std::chrono::milliseconds delay)
    {
        sleepMs.store(delay.count());
        previous = bsv::detail::SetInputHookForTesting(&SlowInputs::Hook);
    }
    ~SlowInputs() { bsv::detail::SetInputHookForTesting(previous); }
    SlowInputs(const SlowInputs&) = delete;
    SlowInputs& operator=(const SlowInputs&) = delete;

private:
    static void Hook(size_t, bsv::detail::InputEvent event)
    {
        if (event == bsv::detail::InputEvent::Claimed)
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs.load()));
    }
    static inline std::atomic<long long> sleepMs{ 0 };
    bsv::detail::InputHook previous = nullptr;
};

// Inputs verified on helpers while validating `etxBin` up to `calls` times,
// stopping at the first call that used a helper.
static uint64_t HelperInputsWithin(const bsv::CTxValidator& se, const std::vector<uint8_t>& etxBin,
                                   size_t nInputs, int calls)
{
    const uint64_t before = bsv::detail::HelperVerifiedInputCountForTesting();
    for (int call = 0; call < calls && bsv::detail::HelperVerifiedInputCountForTesting() == before; ++call)
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
    return bsv::detail::HelperVerifiedInputCountForTesting() - before;
}

// At the default threshold: one input below it never touches the pool; a tx
// exactly at it is offered a helper, and one takes inputs within a few calls.
// The same at a raised threshold.
BOOST_AUTO_TEST_CASE(threshold_boundary_decides_whether_the_pool_is_used, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator se("main");
    for (const size_t threshold : { bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD, size_t{ 40 } }) {
        const InputThresholdGuard guard(threshold);
        BOOST_REQUIRE(PoolIsIdle());
        const auto below = SignedP2PKHBin(threshold - 1);
        BOOST_CHECK_MESSAGE(HelperInputsWithin(se, below, threshold - 1, 50) == 0,
            "threshold=" << threshold << ": a tx below the threshold used a helper");
        const auto at = SignedP2PKHBin(threshold);
        BOOST_CHECK_MESSAGE(HelperInputsWithin(se, at, threshold, 200) > 0,
            "threshold=" << threshold << ": a tx at the threshold never used a helper in 200 calls");
    }
}

// After many concurrent parallel calls the budget is back to zero and no job
// is left queued: callers and helpers both give their core back, and each
// caller withdraws its job before returning.
BOOST_AUTO_TEST_CASE(budget_and_queue_return_to_idle_after_concurrent_calls, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(120);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    std::atomic<size_t> queuedAfterReturn{ 0 };
    std::atomic<size_t> failures{ 0 };
    std::vector<std::thread> callers;
    for (size_t t = 0; t < 6; ++t) {
        callers.emplace_back([&] {
            for (int r = 0; r < 10; ++r) {
                if (!bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, 120, /*consensus=*/true)))
                    failures.fetch_add(1);
            }
        });
    }
    for (auto& caller : callers)
        caller.join();
    BOOST_CHECK_EQUAL(failures.load(), 0u);
    BOOST_CHECK_MESSAGE(PoolIsIdle(), "busy threads stuck at " << bsv::detail::BusyThreadCountForTesting());

    // On one thread with nothing else running, the queue is empty the moment
    // a call returns.
    for (int r = 0; r < 20; ++r) {
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, 120, /*consensus=*/true)));
        if (bsv::detail::QueuedJobCountForTesting() != 0)
            queuedAfterReturn.fetch_add(1);
    }
    BOOST_CHECK_EQUAL(queuedAfterReturn.load(), 0u);
}

// A pool whose helpers are all asleep wakes for a new job.
BOOST_AUTO_TEST_CASE(idle_pool_wakes_for_a_new_job, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(400);
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));  // helpers back asleep
    BOOST_CHECK_MESSAGE(HelperInputsWithin(se, etxBin, 400, 20) > 0, "no helper woke for a new job in 20 calls");
}

// Spends of a bare 1-of-N multisig signed by its first key. CHECKMULTISIG
// walks the keys from the last pushed, so each input checks the signature
// against N - 1 wrong keys first and takes on the order of tens of
// milliseconds. Verified with VerifyScript in consensus
// mode (no post-Genesis key-count limit).
static bsv::CMutableTransactionExtended MakeSlowMultisigSpend(size_t nInputs, size_t nKeys)
{
    const SigHashType allForkId = SigHashType().withForkId();
    std::vector<CKey> keys(nKeys);
    CScript lockingScript = CScript() << OP_1;
    for (size_t k = 0; k < nKeys; ++k) {
        std::array<uint8_t, 32> raw{};
        raw[29] = 0x5a;
        raw[30] = static_cast<uint8_t>((k + 1) >> 8);
        raw[31] = static_cast<uint8_t>(k + 1);
        keys[k].Set(raw.begin(), raw.end(), /*fCompressedIn=*/true);
        lockingScript << ToByteVector(keys[k].GetPubKey());
    }
    lockingScript << static_cast<int64_t>(nKeys) << OP_CHECKMULTISIG;

    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.vin.resize(nInputs);
    eTx.vutxo.resize(nInputs);
    for (size_t i = 0; i < nInputs; ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("02"), static_cast<uint32_t>(i));
        eTx.vutxo[i].nValue = Amount(P2PKH_INPUT_SATOSHIS);
        eTx.vutxo[i].scriptPubKey = lockingScript;
    }
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(P2PKH_OUTPUT_SATOSHIS);
    eTx.mtx.vout[0].scriptPubKey = MakeP2PKHScript(MakeP2PKHTestKey().GetPubKey());
    const CTransaction unsignedTx(eTx.mtx);
    const PrecomputedTransactionData txdata(unsignedTx);
    for (size_t i = 0; i < nInputs; ++i) {
        const uint256 sighash = SignatureHash(lockingScript, unsignedTx, static_cast<unsigned int>(i), allForkId,
            eTx.vutxo[i].nValue, &txdata);
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(keys.front().Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(allForkId.getRawSigHashType()));
        eTx.mtx.vin[i].scriptSig = CScript() << OP_0 << sig;
    }
    return eTx;
}

// While a job runs, more threads than cores become busy: the helpers on it
// leave before claiming another input, and the caller carries on alone. When
// the cores free up, helpers rejoin the same job. Observed through the busy
// count (the caller plus every helper on the job). Each input sleeps 25 ms
// (SlowInputs) and the job gets at most 3 helpers whatever the pool size, so
// it outlasts every step on a loaded many-core machine, and on a small one
// the test thread still runs while the job does.
BOOST_AUTO_TEST_CASE(helpers_leave_an_oversubscribed_machine_and_return, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const size_t cores = bsv::detail::InputPoolHelperCountForTesting() + 1;
    constexpr size_t nInputs = 32;
    const auto etxBin = SignedP2PKHBin(nInputs);
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const SlowInputs slow(std::chrono::milliseconds(25));

    const auto busy = [] { return bsv::detail::BusyThreadCountForTesting(); };
    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread caller([&] {
        status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        done.store(true);
    });

    // The caller and at least one helper are on the job.
    const bool joined = WaitFor([&] { return busy() > 1 || done.load(); }) && !done.load();

    // Count every core as busy on top. From then on no helper can take a
    // core, so the helpers counted now are the only ones that may still
    // verify an input: each finishes at most the one it is verifying or has
    // just claimed, then sees the count over the cores before claiming again.
    // The count settles at the caller plus the extra cores and stays there.
    bsv::detail::ScopedBusyThreadsForTesting oversubscribed(cores);
    const uint64_t helperInputsAtTrip = bsv::detail::HelperVerifiedInputCountForTesting();
    const size_t helpersAtTrip = busy() - 1 - cores;
    const size_t alone = 1 + cores;
    const bool left = WaitFor([&] { return busy() == alone || done.load(); }) && !done.load();
    const uint64_t helperInputsAfterTrip = bsv::detail::HelperVerifiedInputCountForTesting() - helperInputsAtTrip;
    size_t mostWhileOversubscribed = 0;
    for (int sample = 0; sample < 50 && !done.load(); ++sample) {
        mostWhileOversubscribed = std::max(mostWhileOversubscribed, busy());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool stillRunning = !done.load();

    // Cores free again: a helper is woken and rejoins the running job.
    oversubscribed.Release();
    const bool rejoined = WaitFor([&] { return busy() > 1 || done.load(); }) && !done.load();
    caller.join();

    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_REQUIRE_MESSAGE(joined, "no helper joined the job before it finished");
    BOOST_CHECK_MESSAGE(left, "helpers stayed on the job with more threads than cores busy");
    BOOST_CHECK_MESSAGE(helperInputsAfterTrip <= helpersAtTrip,
        helperInputsAfterTrip << " helper inputs after the budget tripped, with " << helpersAtTrip << " helpers on the job");
    BOOST_REQUIRE_MESSAGE(stillRunning, "the job finished during the oversubscribed window");
    BOOST_CHECK_MESSAGE(mostWhileOversubscribed == alone,
        "a helper came back while oversubscribed: busy reached " << mostWhileOversubscribed << ", expected " << alone);
    BOOST_CHECK_MESSAGE(rejoined, "no helper rejoined the job after the cores were released");
}

// Once the first input fails, participants stop claiming: almost none of the
// other inputs are verified, counting the caller's and the helpers'. Input 0
// fails fast (an empty signature needs no ECDSA), and the fewest over five
// calls is what counts: a participant preempted while verifying input 0 lets
// the others run on legitimately.
BOOST_AUTO_TEST_CASE(participants_stop_above_the_lowest_failure, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    constexpr size_t nInputs = 2000;
    const bsv::CTxValidator se("main");
    auto eTx = MakeSignedP2PKHSpend(std::vector<SigHashType>(nInputs, SigHashType().withForkId()),
        /*nOutputs=*/1, /*precomputeForSigning=*/true);
    EmptyP2PKHSignature(eTx, 0);
    const auto etxBin = SerializeExtended(eTx);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    uint64_t fewest = std::numeric_limits<uint64_t>::max();
    for (int call = 0; call < 5; ++call) {
        const uint64_t before = bsv::detail::JobVerifiedInputCountForTesting();
        const TxError status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        BOOST_CHECK(SameTxError(status, bsv::TxErrorScript(SCRIPT_ERR_EVAL_FALSE)));
        fewest = std::min(fewest, bsv::detail::JobVerifiedInputCountForTesting() - before);
    }
    BOOST_CHECK_MESSAGE(fewest < nInputs / 4, "at least " << fewest << " of " << nInputs
        << " inputs were verified in every call after input 0 failed");
}

// A finished job queued behind a job that still has inputs left is not left
// in the queue: its caller withdraws it. NextJob only drops finished jobs
// from the front, so without the withdrawal it would wait for the live one.
// A, at the threshold, is offered one helper; B, started while A runs, is
// offered helpers too, but they join A at the front, and B's caller finishes
// B alone. Needs a free core for B to be offered, so 4 or more cores, and at
// most 8: with more, enough helpers join A to claim all its inputs and A
// leaves the queue early. ctest's test_txvalidator_parallel_inputs runs it
// with the pool capped at 4.
BOOST_AUTO_TEST_CASE(finished_job_behind_a_live_one_leaves_the_queue, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const size_t cores = bsv::detail::InputPoolHelperCountForTesting() + 1;
    if (cores < 4 || cores > 8) {
        BOOST_TEST_MESSAGE(cores << " cores: the queue order this test needs cannot be set up; nothing to check");
        return;
    }
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    const size_t slowInputs = bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD;
    const auto slow = SerializeExtended(MakeSlowMultisigSpend(slowInputs, 400));
    const std::vector<int32_t> slowHeights(slowInputs, P2PKH_UTXO_HEIGHT);
    constexpr size_t fastInputs = 400;
    const auto fast = SignedP2PKHBin(fastInputs);
    const bsv::CTxValidator se("main");
    BOOST_REQUIRE(PoolIsIdle());
    std::atomic<bool> slowDone{ false };
    TxError slowStatus = bsv::TxErrorOk();
    std::thread slowCaller([&] {
        slowStatus = se.VerifyScript(std::span<const uint8_t>(slow), std::span<const int32_t>(slowHeights),
            P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
        slowDone.store(true);
    });
    BOOST_REQUIRE(WaitFor([&] { return bsv::detail::BusyThreadCountForTesting() > 1 || slowDone.load(); }));

    // While A runs, only B's caller can finish a job's Work(): A's own
    // participants report when A ends. So inputs counted inside jobs during
    // B's call mean B ran as a queued job, not serially.
    const uint64_t jobInputsBefore = bsv::detail::JobVerifiedInputCountForTesting();
    const TxError fastStatus = ValidateP2PKHSpend(se, fast, fastInputs, /*consensus=*/true);
    const uint64_t fastJobInputs = bsv::detail::JobVerifiedInputCountForTesting() - jobInputsBefore;
    const size_t queuedAfterFast = bsv::detail::QueuedJobCountForTesting();
    const bool slowStillRunning = !slowDone.load();
    slowCaller.join();

    BOOST_CHECK(bsv::TxErrorIsOk(slowStatus));
    BOOST_CHECK(bsv::TxErrorIsOk(fastStatus));
    BOOST_REQUIRE_MESSAGE(slowStillRunning, "the slow job finished before the fast one");
    BOOST_REQUIRE_MESSAGE(fastJobInputs > 0, "the fast tx was not verified as a job queued behind the slow one");
    BOOST_CHECK_MESSAGE(queuedAfterFast == 1, queuedAfterFast << " jobs queued after the fast call returned; expected only the slow one");
}

// VerifySpend verifies its one input on the calling thread, but that thread
// takes a core: the busy count is 1 while a slow spend runs and 0 after.
BOOST_AUTO_TEST_CASE(verify_spend_counts_against_the_core_budget, PARALLEL_TEST_CASE_DECORATORS)
{
    BOOST_REQUIRE(PoolIsIdle());
    const auto eTx = MakeSlowMultisigSpend(1, 1000);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx.mtx;
    const std::vector<uint8_t> txBin(ss.begin(), ss.end());
    const CTxOut& source = eTx.vutxo[0];
    const bsv::CTxValidator se("main");

    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread spender([&] {
        status = se.VerifySpend(std::span<const uint8_t>(txBin), 0,
            std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
            source.nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
        done.store(true);
    });
    size_t most = 0;
    while (!done.load()) {
        most = std::max(most, bsv::detail::BusyThreadCountForTesting());
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    spender.join();
    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_CHECK_EQUAL(most, 1u);
    BOOST_CHECK_MESSAGE(PoolIsIdle(), "busy count " << bsv::detail::BusyThreadCountForTesting() << " after VerifySpend");
}

// A tx without outputs is verified with BaseSignatureChecker; the parallel
// path returns what the serial one does.
BOOST_AUTO_TEST_CASE(tx_without_outputs_matches_serial, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    auto eTx = MakeParitySpend();
    eTx.mtx.vout.clear();
    const auto etxBin = SerializeExtended(eTx);
    const std::vector<int32_t> utxoHeights(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    for (const bool consensus : { true, false }) {
        TxError results[2];
        for (const size_t threshold : { FORCE_PARALLEL, FORCE_SERIAL }) {
            const InputThresholdGuard guard(threshold);
            results[threshold == FORCE_SERIAL] = se.VerifyScript(std::span<const uint8_t>(etxBin),
                std::span<const int32_t>(utxoHeights), P2PKH_BLOCK_HEIGHT, consensus);
        }
        BOOST_CHECK_MESSAGE(SameTxError(results[0], results[1]),
            "consensus=" << consensus << ": parallel " << results[0].domain << "/" << results[0].code
            << " serial " << results[1].domain << "/" << results[1].code);
        BOOST_CHECK(!bsv::TxErrorIsOk(results[1]));  // no checker can verify the signatures
    }
}

// Fixes the order in which failing inputs of a parallel job are recorded:
// every index in `order` waits on its verifying thread until all of them have
// failed, then they are released one at a time in `order`. Each index needs
// its own participant, so the job must have as many participants as indices.
class FailureOrder {
public:
    explicit FailureOrder(std::vector<size_t> order)
    {
        std::lock_guard lock{ mutex };
        sequence = std::move(order);
        arrived.clear();
        released = 0;
        timedOut = false;
        previous = bsv::detail::SetInputHookForTesting(&FailureOrder::Hook);
    }
    ~FailureOrder() { bsv::detail::SetInputHookForTesting(previous); }
    FailureOrder(const FailureOrder&) = delete;
    FailureOrder& operator=(const FailureOrder&) = delete;

    // Whether the last call released every index in order (no participant
    // was missing, so nothing timed out).
    static bool Achieved()
    {
        std::lock_guard lock{ mutex };
        return !timedOut && released == sequence.size();
    }
    static void Reset()
    {
        std::lock_guard lock{ mutex };
        arrived.clear();
        released = 0;
        timedOut = false;
    }

private:
    static void Hook(size_t index, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Failed)
            return;
        std::unique_lock lock{ mutex };
        if (std::find(sequence.begin(), sequence.end(), index) == sequence.end())
            return;
        arrived.push_back(index);
        turn.notify_all();
        const bool inOrder = turn.wait_for(lock, std::chrono::seconds(5), [&] {
            return arrived.size() == sequence.size() && released < sequence.size() && sequence[released] == index;
        });
        if (!inOrder) {
            timedOut = true;
            return;
        }
        ++released;
        turn.notify_all();
    }

    static inline std::mutex mutex;
    static inline std::condition_variable turn;
    static inline std::vector<size_t> sequence;
    static inline std::vector<size_t> arrived;
    static inline size_t released = 0;
    static inline bool timedOut = false;
    bsv::detail::InputHook previous = nullptr;
};

// Three failing inputs recorded in the order 2, 0, 1: the job keeps lowering
// the lowest failure, so input 0's error is the result, not input 1's that
// arrives after it. Needs three participants (3 or more cores).
BOOST_AUTO_TEST_CASE(failures_recorded_out_of_index_order_keep_the_lowest, PARALLEL_TEST_CASE_DECORATORS)
{
    if (bsv::detail::InputPoolHelperCountForTesting() < 2) {
        BOOST_TEST_MESSAGE("fewer than 2 helpers: three participants cannot hold three inputs at once; nothing to check");
        return;
    }
    const bsv::CTxValidator se("main");
    auto eTx = MakeSignedP2PKHSpend(std::vector<SigHashType>(3, SigHashType().withForkId()), 2);
    EmptyP2PKHSignature(eTx, 0);                                               // EVAL_FALSE
    CorruptP2PKHSignature(eTx, 1);                                             // NULLFAIL
    EditP2PKHScriptSig(eTx, 2, [](auto&, auto& pubkey) {                       // EQUALVERIFY
        pubkey = ToByteVector(MakeDeterministicTestKey(0x70).GetPubKey());
    });
    const auto etxBin = SerializeExtended(eTx);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const FailureOrder order({ 2, 0, 1 });
    for (int call = 0; call < 5; ++call) {
        FailureOrder::Reset();
        const TxError status = ValidateP2PKHSpend(se, etxBin, 3, /*consensus=*/true);
        BOOST_REQUIRE_MESSAGE(FailureOrder::Achieved(), "call " << call << ": the three failures were not recorded in the order 2, 0, 1");
        BOOST_CHECK_MESSAGE(SameTxError(status, bsv::TxErrorScript(SCRIPT_ERR_EVAL_FALSE)),
            "call " << call << ": " << status.domain << "/" << status.code << ", expected input 0's EVAL_FALSE");
        BOOST_REQUIRE(PoolIsIdle());
    }
}

// An input that throws and one that fails with a TxError, in one job: the
// lower index wins either way, as in the serial loop. A block-validation call
// (consensus) at block height MEMPOOL_HEIGHT gives both: an input with an
// unconfirmed height returns UnconfirmedInputInBlock before its flags are
// computed, and computing a confirmed input's flags for that height throws.
BOOST_AUTO_TEST_CASE(exception_and_failure_keep_index_order, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const TxError unconfirmed = bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::UnconfirmedInputInBlock));
    const auto verify = [&](const std::vector<uint8_t>& etxBin, const std::vector<int32_t>& heights) {
        return se.VerifyScript(std::span<const uint8_t>(etxBin), std::span<const int32_t>(heights),
            MEMPOOL_HEIGHT, /*consensus=*/true);
    };

    // Many inputs, racing: [unconfirmed, confirmed...] and [confirmed, unconfirmed...].
    const auto large = SerializeExtended(MakeParitySpend());
    std::vector<int32_t> unconfirmedFirst(PARITY_INPUTS, P2PKH_UTXO_HEIGHT);
    unconfirmedFirst[0] = MEMPOOL_HEIGHT;
    std::vector<int32_t> confirmedFirst(PARITY_INPUTS, MEMPOOL_HEIGHT);
    confirmedFirst[0] = P2PKH_UTXO_HEIGHT;
    for (const auto& [heights, expected, label] : { std::tuple{ &unconfirmedFirst, unconfirmed, "unconfirmed first" },
                                                    std::tuple{ &confirmedFirst, bsv::TxErrorException(), "confirmed first" } }) {
        {
            const InputThresholdGuard serial(FORCE_SERIAL);
            const TxError status = verify(large, *heights);
            BOOST_REQUIRE_MESSAGE(SameTxError(status, expected), label << ", serial: " << status.domain << "/" << status.code);
        }
        const InputThresholdGuard parallel(FORCE_PARALLEL);
        size_t mismatches = 0;
        for (int call = 0; call < 1000; ++call)
            mismatches += !SameTxError(verify(large, *heights), expected);
        BOOST_CHECK_MESSAGE(mismatches == 0, label << ", parallel: " << mismatches << " of 1000 calls differ from serial");
    }

    // Two inputs, [unconfirmed, confirmed], with the failures recorded in each
    // order: the exception at input 1 neither replaces input 0's failure when
    // it comes second nor survives it when it comes first.
    if (bsv::detail::InputPoolHelperCountForTesting() < 1) {
        BOOST_TEST_MESSAGE("no helper: the two failures cannot be ordered");
        return;
    }
    const auto pair = SerializeExtended(MakeSignedP2PKHSpend(std::vector<SigHashType>(2, SigHashType().withForkId()), 2));
    const std::vector<int32_t> pairHeights{ MEMPOOL_HEIGHT, P2PKH_UTXO_HEIGHT };
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    for (const std::vector<size_t>& sequence : { std::vector<size_t>{ 0, 1 }, std::vector<size_t>{ 1, 0 } }) {
        BOOST_REQUIRE(PoolIsIdle());
        const FailureOrder order(sequence);
        for (int call = 0; call < 3; ++call) {
            FailureOrder::Reset();
            const TxError status = verify(pair, pairHeights);
            BOOST_REQUIRE_MESSAGE(FailureOrder::Achieved(),
                "order " << sequence[0] << "," << sequence[1] << ": the two failures were not recorded in that order");
            BOOST_CHECK_MESSAGE(SameTxError(status, unconfirmed), "order " << sequence[0] << "," << sequence[1]
                << ": " << status.domain << "/" << status.code << ", expected input 0's UnconfirmedInputInBlock");
            BOOST_REQUIRE(PoolIsIdle());
        }
    }
}

// On an idle pool, a large tx gets every helper: each verifies at least one
// of its inputs.
BOOST_AUTO_TEST_CASE(every_helper_joins_a_large_job_on_an_idle_pool, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0) {
        BOOST_TEST_MESSAGE("no helpers on this machine; nothing to check");
        return;
    }
    constexpr size_t nInputs = 8000;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(nInputs);
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    uint64_t most = 0;
    for (int call = 0; call < 5 && most < helpers; ++call) {
        BOOST_REQUIRE(PoolIsIdle());
        const uint64_t before = bsv::detail::HelperJobParticipationCountForTesting();
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
        most = std::max(most, bsv::detail::HelperJobParticipationCountForTesting() - before);
    }
    BOOST_CHECK_MESSAGE(most >= helpers, "at most " << most << " of " << helpers << " helpers verified inputs of one job");
}

// A large tx that starts with every core already counted busy still queues
// its job, offered to no helper: the caller verifies alone and no helper is
// woken to find no core. When a core frees while it runs, a helper joins it.
BOOST_AUTO_TEST_CASE(large_tx_started_with_every_core_busy_takes_a_helper_when_one_frees, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0) {
        BOOST_TEST_MESSAGE("no helpers on this machine; nothing to check");
        return;
    }
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    // A helper that has just left a job re-checks the queue once on its way
    // back to sleep; let that happen before counting wakes.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    {
        constexpr size_t nInputs = 200;
        const auto etxBin = SignedP2PKHBin(nInputs);
        const bsv::detail::ScopedBusyThreadsForTesting everyCore(helpers + 1);
        const uint64_t jobInputs = bsv::detail::JobVerifiedInputCountForTesting();
        const uint64_t helperInputs = bsv::detail::HelperVerifiedInputCountForTesting();
        const uint64_t wakes = bsv::detail::HelperWakesWithoutCoreForTesting();
        BOOST_CHECK(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
        BOOST_CHECK_EQUAL(bsv::detail::JobVerifiedInputCountForTesting() - jobInputs, nInputs);
        BOOST_CHECK_EQUAL(bsv::detail::HelperVerifiedInputCountForTesting() - helperInputs, 0u);
        BOOST_CHECK_EQUAL(bsv::detail::HelperWakesWithoutCoreForTesting() - wakes, 0u);
        BOOST_CHECK_EQUAL(bsv::detail::QueuedJobCountForTesting(), 0u);
    }

    constexpr size_t nInputs = 32;
    const auto etxBin = SignedP2PKHBin(nInputs);
    BOOST_REQUIRE(PoolIsIdle());
    const SlowInputs slow(std::chrono::milliseconds(20));
    bsv::detail::ScopedBusyThreadsForTesting everyCore(helpers + 1);
    const uint64_t helperInputs = bsv::detail::HelperVerifiedInputCountForTesting();
    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread caller([&] {
        status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        done.store(true);
    });
    const bool queued = WaitFor([&] { return bsv::detail::QueuedJobCountForTesting() == 1 || done.load(); }) && !done.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const uint64_t whileBusy = bsv::detail::HelperVerifiedInputCountForTesting() - helperInputs;
    everyCore.Release();
    const bool joined = WaitFor([&] {
        return bsv::detail::HelperVerifiedInputCountForTesting() > helperInputs + whileBusy || done.load();
    }) && !done.load();
    caller.join();

    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_REQUIRE_MESSAGE(queued, "the tx queued no job with every core busy");
    BOOST_CHECK_EQUAL(whileBusy, 0u);
    BOOST_CHECK_MESSAGE(joined, "no helper joined the job after the cores were released");
    BOOST_CHECK(PoolIsIdle());
    BOOST_CHECK_EQUAL(bsv::detail::QueuedJobCountForTesting(), 0u);
}

// Coordinate the missing release-before-publication interval. Waiting events
// acknowledge that every helper has parked; the callback never blocks while
// holding the pool mutex. Only BeforeOffer and claimed inputs may wait.
class ReleaseBeforeOffer {
public:
    ReleaseBeforeOffer()
    {
        active.store(this);
        previousPool = bsv::detail::SetInputPoolHookForTesting(&PoolHook);
        previousInput = bsv::detail::SetInputHookForTesting(&InputHook);
        bsv::detail::WakeInputHelpersForTesting();
    }
    ~ReleaseBeforeOffer()
    {
        bsv::detail::SetInputHookForTesting(previousInput);
        bsv::detail::SetInputPoolHookForTesting(previousPool);
        // Synchronize with any old Waiting callback before destroying its state.
        bsv::detail::WakeInputHelpersForTesting();
        active.store(nullptr);
    }
    size_t ParkedCount()
    {
        std::lock_guard lock{ mutex };
        return parked.size();
    }
    static inline thread_local bool largeCaller = false;
    std::atomic<bool> beforeOffer{ false }, resumeOffer{ false };
    std::atomic<bool> helperClaimed{ false }, releaseInputs{ false }, timedOut{ false };

private:
    static void PoolHook(bsv::detail::InputPoolEvent event)
    {
        auto& self = *active.load();
        if (event == bsv::detail::InputPoolEvent::Waiting) {
            std::lock_guard lock{ self.mutex };
            self.parked[std::this_thread::get_id()] = true;
        } else {
            self.beforeOffer = true;
            if (!WaitFor([&] { return self.resumeOffer.load(); }, std::chrono::milliseconds(20000)))
                self.timedOut = true;
        }
    }
    static void InputHook(size_t, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        auto& self = *active.load();
        if (!largeCaller)
            self.helperClaimed = true;
        if (!WaitFor([&] { return self.releaseInputs.load(); }, std::chrono::milliseconds(20000)))
            self.timedOut = true;
    }
    std::mutex mutex;
    std::map<std::thread::id, bool> parked;
    bsv::detail::InputPoolHook previousPool;
    bsv::detail::InputHook previousInput;
    static inline std::atomic<ReleaseBeforeOffer*> active{ nullptr };
};

// The second VerifySpend token poll is after BusyScope construction. Holding
// there keeps a real ordinary caller on a core; releasing it exercises that
// caller's actual BusyScope destruction and ReleasedCore notification path.
class HeldSpendSource final : public task::CCancellationSource {
public:
    bool IsCanceled() override
    {
        if (++polls == 2) {
            entered = true;
            if (!WaitFor([&] { return release.load(); }, std::chrono::milliseconds(20000)))
                timedOut = true;
        }
        return false;
    }
    std::atomic<bool> entered{ false }, release{ false }, timedOut{ false };
private:
    size_t polls = 0;  // read only by this source's one VerifySpend caller
};

BOOST_AUTO_TEST_CASE(release_before_job_publication_recruits_a_helper, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0)
        return;
    const bsv::CTxValidator validator("main");
    const auto spend = MakeSignedP2PKHSpend({ SigHashType().withForkId() }, 1);
    CDataStream plain(SER_NETWORK, PROTOCOL_VERSION);
    plain << spend.mtx;
    const std::vector<uint8_t> txBin(plain.begin(), plain.end());
    const auto& locking = spend.vutxo[0].scriptPubKey;
    const auto bin = SignedP2PKHBin(200);
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const uint64_t helperInputs = bsv::detail::HelperVerifiedInputCountForTesting();
    ReleaseBeforeOffer order;
    BOOST_REQUIRE(WaitFor([&] { return order.ParkedCount() == helpers; }));

    std::vector<std::shared_ptr<HeldSpendSource>> sources;
    std::vector<TxError> results(helpers, bsv::TxErrorException());
    std::vector<std::thread> busyCallers;
    for (size_t i = 0; i < helpers; ++i) {
        const auto source = std::make_shared<HeldSpendSource>();
        sources.push_back(source);
        busyCallers.emplace_back([&, i, source] {
            results[i] = validator.VerifySpend(txBin, 0,
                std::span<const uint8_t>(locking.data(), locking.size()),
                spend.vutxo[0].nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT,
                P2PKH_BLOCK_HEIGHT, true, std::nullopt, source->GetToken());
        });
    }
    const bool occupied = WaitFor([&] {
        return std::all_of(sources.begin(), sources.end(), [](const auto& s) { return s->entered.load(); });
    });
    TxError result = bsv::TxErrorException();
    std::thread caller([&] {
        ReleaseBeforeOffer::largeCaller = true;
        result = ValidateP2PKHSpend(validator, bin, 200, true);
    });
    const bool sampled = WaitFor([&] { return order.beforeOffer.load(); });
    const size_t busyAtSample = bsv::detail::BusyThreadCountForTesting();
    const size_t queuedBefore = bsv::detail::QueuedJobCountForTesting();
    for (const auto& source : sources)
        source->release = true;
    for (auto& worker : busyCallers)
        worker.join();
    const size_t busyAfterRelease = bsv::detail::BusyThreadCountForTesting();
    const size_t queuedAfterRelease = bsv::detail::QueuedJobCountForTesting();
    order.resumeOffer = true;
    const bool recruited = WaitFor([&] { return order.helperClaimed.load(); }, std::chrono::milliseconds(1000));
    order.releaseInputs = true;
    caller.join();

    BOOST_REQUIRE(occupied);
    BOOST_REQUIRE(sampled);
    BOOST_CHECK_EQUAL(busyAtSample, helpers + 1);
    BOOST_CHECK_EQUAL(queuedBefore, 0u);
    BOOST_CHECK_EQUAL(busyAfterRelease, 1u);
    BOOST_CHECK_EQUAL(queuedAfterRelease, 0u);
    for (size_t i = 0; i < helpers; ++i) {
        BOOST_CHECK(!sources[i]->timedOut);
        BOOST_CHECK(bsv::TxErrorIsOk(results[i]));
    }
    BOOST_CHECK(!order.timedOut);
    BOOST_CHECK(bsv::TxErrorIsOk(result));
    BOOST_CHECK_MESSAGE(recruited, "no helper joined after capacity was released before job publication");
    BOOST_CHECK_GT(bsv::detail::HelperVerifiedInputCountForTesting(), helperInputs);
    BOOST_CHECK(PoolIsIdle());
}

// Helpers parked over budget come back when exactly one core frees: the
// decrement that takes the busy count from the core count to one below it
// wakes a helper, which rejoins the running job.
BOOST_AUTO_TEST_CASE(freeing_one_core_wakes_a_parked_helper, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0) {
        BOOST_TEST_MESSAGE("no helpers on this machine; nothing to check");
        return;
    }
    const size_t cores = helpers + 1;
    constexpr size_t nInputs = 32;
    const auto etxBin = SignedP2PKHBin(nInputs);
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const SlowInputs slow(std::chrono::milliseconds(25));

    const auto busy = [] { return bsv::detail::BusyThreadCountForTesting(); };
    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread caller([&] {
        status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        done.store(true);
    });
    const bool joined = WaitFor([&] { return busy() > 1 || done.load(); }) && !done.load();

    // Every core counted busy on top: the helpers leave and park.
    bsv::detail::AddBusyThreadsForTesting(cores);
    const bool parked = WaitFor([&] { return busy() == 1 + cores || done.load(); }) && !done.load();
    // Free one core: still every core busy, nothing may wake.
    bsv::detail::ReleaseBusyThreadsForTesting(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const size_t atCores = busy();
    // Free one more: one core is idle, so a helper must take it.
    bsv::detail::ReleaseBusyThreadsForTesting(1);
    const bool resumed = WaitFor([&] { return busy() == cores || done.load(); }, std::chrono::milliseconds(2000))
        && !done.load();
    bsv::detail::ReleaseBusyThreadsForTesting(cores - 2);
    caller.join();

    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_REQUIRE_MESSAGE(joined, "no helper joined the slow job");
    BOOST_REQUIRE_MESSAGE(parked, "the helpers did not leave with every core busy");
    BOOST_CHECK_EQUAL(atCores, cores);
    BOOST_CHECK_MESSAGE(resumed, "no helper took the freed core within 2 s (busy " << busy() << ")");
}

// Holds both callers in their inputs and the first helper after it releases
// its core. Only another helper can then claim the queued job's remaining input.
class HelperReleaseOrder {
public:
    HelperReleaseOrder()
    {
        firstCallerClaimed = secondCallerClaimed = firstHelperClaimed = false;
        firstHelperReleased = queuedHelperClaimed = releaseFirstHelper = releaseAll = false;
        previousInput = bsv::detail::SetInputHookForTesting(&OnInput);
        previousRelease = bsv::detail::SetHelperReleasedCoreHookForTesting(&OnRelease);
    }
    ~HelperReleaseOrder()
    {
        releaseFirstHelper = releaseAll = true;
        bsv::detail::SetInputHookForTesting(previousInput);
        bsv::detail::SetHelperReleasedCoreHookForTesting(previousRelease);
    }
    HelperReleaseOrder(const HelperReleaseOrder&) = delete;
    HelperReleaseOrder& operator=(const HelperReleaseOrder&) = delete;

    static inline thread_local unsigned caller = 0;
    static inline std::atomic<bool> firstCallerClaimed{ false }, secondCallerClaimed{ false };
    static inline std::atomic<bool> firstHelperClaimed{ false }, firstHelperReleased{ false };
    static inline std::atomic<bool> queuedHelperClaimed{ false }, releaseFirstHelper{ false }, releaseAll{ false };

private:
    static void OnInput(size_t, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        if (caller == 1)
            firstCallerClaimed = true;
        else if (caller == 2)
            secondCallerClaimed = true;
        else if (!firstHelperClaimed.exchange(true)) {
            firstHelper = true;
            WaitFor([] { return releaseFirstHelper.load(); }, std::chrono::milliseconds(20000));
            return;
        } else
            queuedHelperClaimed = true;
        WaitFor([] { return releaseAll.load(); }, std::chrono::milliseconds(20000));
    }

    static void OnRelease()
    {
        if (!firstHelper)
            return;
        firstHelper = false;
        firstHelperReleased = true;
        WaitFor([] { return releaseAll.load(); }, std::chrono::milliseconds(20000));
    }

    static inline thread_local bool firstHelper = false;
    bsv::detail::InputHook previousInput = nullptr;
    bsv::detail::HelperReleasedCoreHook previousRelease = nullptr;
};

BOOST_AUTO_TEST_CASE(leaving_helper_wakes_another_for_a_queued_job, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers < 2) {
        BOOST_TEST_MESSAGE("needs at least two helpers; nothing to check");
        return;
    }
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(2);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const HelperReleaseOrder order;
    // Leave room for two callers and one helper, filling the core budget
    // when the second caller queues its job without waking any helper.
    bsv::detail::ScopedBusyThreadsForTesting extra(helpers - 2);
    TxError firstStatus = bsv::TxErrorOk(), secondStatus = bsv::TxErrorOk();
    std::thread first([&] {
        HelperReleaseOrder::caller = 1;
        firstStatus = ValidateP2PKHSpend(se, etxBin, 2, /*consensus=*/true);
    });
    const bool joined = WaitFor([] {
        return HelperReleaseOrder::firstCallerClaimed && HelperReleaseOrder::firstHelperClaimed;
    });
    std::thread second([&] {
        HelperReleaseOrder::caller = 2;
        secondStatus = ValidateP2PKHSpend(se, etxBin, 2, /*consensus=*/true);
    });
    const bool queued = WaitFor([] { return HelperReleaseOrder::secondCallerClaimed.load(); });
    const size_t busyBefore = bsv::detail::BusyThreadCountForTesting();
    const bool helpedBeforeRelease = HelperReleaseOrder::queuedHelperClaimed;
    HelperReleaseOrder::releaseFirstHelper = true;
    const bool released = WaitFor([] { return HelperReleaseOrder::firstHelperReleased.load(); });
    const bool woke = WaitFor([] { return HelperReleaseOrder::queuedHelperClaimed.load(); });
    // Unblock and join before asserting, including on the missing-wakeup path.
    HelperReleaseOrder::releaseAll = true;
    first.join();
    second.join();
    extra.Release();

    BOOST_REQUIRE_MESSAGE(joined, "the first job did not hold its caller and helper");
    BOOST_REQUIRE_MESSAGE(queued, "the second caller did not queue its job");
    BOOST_CHECK_EQUAL(busyBefore, helpers + 1);
    BOOST_CHECK(!helpedBeforeRelease);
    BOOST_REQUIRE_MESSAGE(released, "the first helper did not release its core");
    BOOST_CHECK_MESSAGE(woke, "no other helper woke for the queued job after a helper released its core");
    BOOST_CHECK(bsv::TxErrorIsOk(firstStatus));
    BOOST_CHECK(bsv::TxErrorIsOk(secondStatus));
    BOOST_CHECK(PoolIsIdle());
    BOOST_CHECK_EQUAL(bsv::detail::QueuedJobCountForTesting(), 0u);
}

// A helper keeps working while the busy count equals the core count, its
// own core included: only a count above the core count sends it away. One
// helper works a job of slow inputs (40 ms each, SlowInputs); the count is
// raised to exactly the core count, and the helper must verify at least two
// more inputs (the one in flight would finish either way).
BOOST_AUTO_TEST_CASE(helper_stays_with_the_busy_count_at_the_core_count, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0) {
        BOOST_TEST_MESSAGE("no helpers on this machine; nothing to check");
        return;
    }
    const size_t cores = helpers + 1;
    const size_t nInputs = bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD;
    const auto etxBin = SignedP2PKHBin(nInputs);
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const SlowInputs slow(std::chrono::milliseconds(40));

    const auto busy = [] { return bsv::detail::BusyThreadCountForTesting(); };
    const auto helperInputs = [] { return bsv::detail::HelperVerifiedInputCountForTesting(); };
    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread caller([&] {
        status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        done.store(true);
    });
    // A job at the threshold is offered one helper: the caller and it are busy.
    const bool joined = WaitFor([&] { return busy() == 2 || done.load(); }) && !done.load();
    const size_t extra = cores - 2;
    bsv::detail::AddBusyThreadsForTesting(extra);
    const size_t atCores = busy();
    const uint64_t before = helperInputs();
    const bool kept = WaitFor([&] { return helperInputs() >= before + 2 || done.load(); }, std::chrono::milliseconds(3000))
        && helperInputs() >= before + 2;
    bsv::detail::ReleaseBusyThreadsForTesting(extra);
    caller.join();

    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_REQUIRE_MESSAGE(joined, "the slow job did not get exactly one helper");
    BOOST_REQUIRE_EQUAL(atCores, cores);
    BOOST_CHECK_MESSAGE(kept, "the helper verified " << helperInputs() - before
        << " more inputs with the busy count at the core count; expected at least 2");
}

// Records, from the claim hook, which thread claimed each input of a parallel
// job and that thread's stack size; optionally holds the caller on input 0 so
// helpers claim the rest.
class ClaimRecorder {
public:
    struct Claim {
        std::thread::id thread;
        size_t stackBytes;
    };

    explicit ClaimRecorder(bool holdCallerOnFirstInput)
    {
        std::lock_guard lock{ mutex };
        claims.clear();
        caller = std::this_thread::get_id();
        hold = holdCallerOnFirstInput;
        previous = bsv::detail::SetInputHookForTesting(&ClaimRecorder::Hook);
    }
    ~ClaimRecorder() { bsv::detail::SetInputHookForTesting(previous); }
    ClaimRecorder(const ClaimRecorder&) = delete;
    ClaimRecorder& operator=(const ClaimRecorder&) = delete;

    static std::map<size_t, Claim> Claims()
    {
        std::lock_guard lock{ mutex };
        return claims;
    }
    static void Reset()
    {
        std::lock_guard lock{ mutex };
        claims.clear();
    }
    static bool OnHelper(size_t index)
    {
        std::lock_guard lock{ mutex };
        const auto it = claims.find(index);
        return it != claims.end() && it->second.thread != caller;
    }

private:
    static size_t CurrentStackBytes()
    {
#if defined(__APPLE__)
        return pthread_get_stacksize_np(pthread_self());
#elif defined(__linux__)
        pthread_attr_t attr;
        size_t bytes = 0;
        if (pthread_getattr_np(pthread_self(), &attr) == 0) {
            pthread_attr_getstacksize(&attr, &bytes);
            pthread_attr_destroy(&attr);
        }
        return bytes;
#else
        return 0;
#endif
    }

    static void Hook(size_t index, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        bool holdHere = false;
        {
            std::lock_guard lock{ mutex };
            claims[index] = Claim{ std::this_thread::get_id(), CurrentStackBytes() };
            holdHere = hold && index == 0 && std::this_thread::get_id() == caller;
        }
        if (holdHere)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    static inline std::mutex mutex;
    static inline std::map<size_t, Claim> claims;
    static inline std::thread::id caller;
    static inline bool hold = false;
    bsv::detail::InputHook previous = nullptr;
};

// The test-only counters (inputs, participations, source constructions)
// stay untouched unless a test turns them on, so production does no
// per-input write to them; helpers still verify inputs either way.
BOOST_AUTO_TEST_CASE(input_counters_count_only_when_enabled, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    constexpr size_t nInputs = 200;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(nInputs);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const auto counters = [] {
        return std::array<uint64_t, 4>{ bsv::detail::HelperVerifiedInputCountForTesting(),
                                        bsv::detail::JobVerifiedInputCountForTesting(),
                                        bsv::detail::HelperJobParticipationCountForTesting(),
                                        bsv::detail::InputCancellationSourceCountForTesting() };
    };
    const ClaimRecorder recorder(/*holdCallerOnFirstInput=*/true);
    {
        const bsv::detail::ScopedInputCountersForTesting off(false);
        const auto before = counters();
        bool helperVerified = false;
        for (int call = 0; call < 5 && !helperVerified; ++call) {
            ClaimRecorder::Reset();
            BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
            for (size_t i = 1; i < nInputs && !helperVerified; ++i)
                helperVerified = ClaimRecorder::OnHelper(i);
        }
        BOOST_REQUIRE_MESSAGE(helperVerified, "no helper verified an input with the counters off");
        BOOST_CHECK_MESSAGE(counters() == before, "a test-only counter moved with counting off");
    }
    {
        const bsv::detail::ScopedInputCountersForTesting on(true);
        const auto before = counters();
        BOOST_REQUIRE(PoolIsIdle());
        ClaimRecorder::Reset();
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
        BOOST_CHECK_GT(counters()[1], before[1]);
        BOOST_CHECK_GT(counters()[3], before[3]);
    }
}

// Hold each participant's first two claims until every participant reaches
// that round. This makes reuse observable without relying on scheduling speed.
class TwoClaimsPerParticipant {
public:
    explicit TwoClaimsPerParticipant(size_t participants) : participants{ participants }
    {
        active = this;
        previous = bsv::detail::SetInputHookForTesting(&Hook);
    }
    ~TwoClaimsPerParticipant() { bsv::detail::SetInputHookForTesting(previous); }
    std::map<std::thread::id, size_t> claims;
    bool timedOut = false;

private:
    static void Hook(size_t, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        auto& self = *active;
        std::unique_lock lock{ self.mutex };
        const size_t round = ++self.claims[std::this_thread::get_id()];
        self.changed.notify_all();
        if (round <= 2 && !self.changed.wait_for(lock, std::chrono::seconds(5), [&] {
                return self.claims.size() == self.participants
                    && std::all_of(self.claims.begin(), self.claims.end(),
                        [round](const auto& entry) { return entry.second >= round; });
            }))
            self.timedOut = true;
    }
    const size_t participants;
    std::mutex mutex;
    std::condition_variable changed;
    bsv::detail::InputHook previous;
    static inline TwoClaimsPerParticipant* active = nullptr;
};

BOOST_AUTO_TEST_CASE(cancellation_source_is_reused_for_each_participant, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers == 0)
        return;
    const size_t participants = helpers + 1;
    const size_t nInputs = participants * 8;
    const bsv::CTxValidator validator("main");
    const auto bin = SignedP2PKHBin(nInputs);
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const auto before = bsv::detail::InputCancellationSourceCountForTesting();
    const auto helperCallsBefore = bsv::detail::HelperJobParticipationCountForTesting();
    const TwoClaimsPerParticipant rounds(participants);
    BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(validator, bin, nInputs, true)));
    BOOST_REQUIRE(PoolIsIdle());
    BOOST_REQUIRE(!rounds.timedOut);
    BOOST_REQUIRE_EQUAL(rounds.claims.size(), participants);
    for (const auto& [thread, claims] : rounds.claims)
        BOOST_CHECK_GE(claims, 2u);
    const auto nonemptyCalls = bsv::detail::HelperJobParticipationCountForTesting() - helperCallsBefore + 1;
    const auto sources = bsv::detail::InputCancellationSourceCountForTesting() - before;
    BOOST_CHECK_EQUAL(nonemptyCalls, participants);
    BOOST_CHECK_EQUAL(sources, nonemptyCalls);
    BOOST_CHECK_LT(sources, nInputs);
    BOOST_TEST_MESSAGE(sources << " sources for " << nonemptyCalls << " nonempty Work calls and " << nInputs << " inputs");
}

// Helper threads get an 8 MiB stack, as a caller's main thread usually has,
// rather than the platform default (512 KiB for secondary threads on macOS).
BOOST_AUTO_TEST_CASE(helpers_run_on_an_8_mib_stack, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
#if !defined(__APPLE__) && !defined(__linux__)
    BOOST_TEST_MESSAGE("no stack size query on this platform; nothing to check");
    return;
#endif
    constexpr size_t nInputs = 64;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(nInputs);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const ClaimRecorder recorder(/*holdCallerOnFirstInput=*/true);
    size_t helperClaims = 0;
    size_t smallest = std::numeric_limits<size_t>::max();
    for (int call = 0; call < 5 && helperClaims == 0; ++call) {
        ClaimRecorder::Reset();
        BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
        for (const auto& [index, claim] : ClaimRecorder::Claims()) {
            if (ClaimRecorder::OnHelper(index)) {
                ++helperClaims;
                smallest = std::min(smallest, claim.stackBytes);
            }
        }
    }
    BOOST_REQUIRE_MESSAGE(helperClaims > 0, "no helper claimed an input");
    BOOST_CHECK_MESSAGE(smallest >= (size_t{ 8 } << 20), "a helper ran on a " << smallest << "-byte stack");
}

// Inputs (0) P2PKH, (1) OP_IF nested `depth` deep, (2) a bare 1-of-`nKeys`
// multisig signed by the key CHECKMULTISIG reaches last. Verified with
// VerifyScript in consensus mode (no post-Genesis size, op or key-count
// limits).
static bsv::CMutableTransactionExtended MakeDeepScriptSpend(size_t depth, size_t nKeys)
{
    auto eTx = MakeSignedP2PKHSpend(std::vector<SigHashType>(3, SigHashType().withForkId()), 1);
    CScript nested;
    for (size_t i = 0; i < depth; ++i)
        nested << OP_1 << OP_IF;
    nested << OP_1;
    for (size_t i = 0; i < depth; ++i)
        nested << OP_ENDIF;
    eTx.vutxo[1].scriptPubKey = nested;
    eTx.mtx.vin[1].scriptSig = CScript();

    std::vector<CKey> keys(nKeys);
    CScript multisig = CScript() << OP_1;
    for (size_t k = 0; k < nKeys; ++k) {
        std::array<uint8_t, 32> raw{};
        raw[29] = 0x6b;
        raw[30] = static_cast<uint8_t>((k + 1) >> 8);
        raw[31] = static_cast<uint8_t>(k + 1);
        keys[k].Set(raw.begin(), raw.end(), /*fCompressedIn=*/true);
        multisig << ToByteVector(keys[k].GetPubKey());
    }
    multisig << static_cast<int64_t>(nKeys) << OP_CHECKMULTISIG;
    eTx.vutxo[2].scriptPubKey = multisig;
    const CTransaction unsignedTx(eTx.mtx);
    const SigHashType allForkId = SigHashType().withForkId();
    const uint256 sighash = SignatureHash(multisig, unsignedTx, 2, allForkId, eTx.vutxo[2].nValue);
    std::vector<uint8_t> sig;
    BOOST_REQUIRE(keys.front().Sign(sighash, sig));
    sig.push_back(static_cast<uint8_t>(allForkId.getRawSigHashType()));
    eTx.mtx.vin[2].scriptSig = CScript() << OP_0 << sig;
    return eTx;
}

constexpr size_t DEEP_IF_DEPTH = 100000;
constexpr size_t DEEP_MULTISIG_KEYS = 1000;

// The deepest scripts in this suite, an OP_IF nested 100,000 deep and a
// 1-of-1,000 multisig checked against every key, verify on a helper thread.
BOOST_AUTO_TEST_CASE(deep_scripts_verify_on_a_helper, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    const bsv::CTxValidator se("main");
    const auto eTx = MakeDeepScriptSpend(DEEP_IF_DEPTH, DEEP_MULTISIG_KEYS);
    const auto etxBin = SerializeExtended(eTx);
    const std::vector<int32_t> heights(3, P2PKH_UTXO_HEIGHT);
    const auto verify = [&] {
        return se.VerifyScript(std::span<const uint8_t>(etxBin), std::span<const int32_t>(heights),
            P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
    };
    {
        const InputThresholdGuard serial(FORCE_SERIAL);
        const TxError status = verify();
        BOOST_REQUIRE_MESSAGE(bsv::TxErrorIsOk(status), "serial: " << status.domain << "/" << status.code);
    }
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const ClaimRecorder recorder(/*holdCallerOnFirstInput=*/true);
    bool bothOnHelpers = false;
    for (int call = 0; call < 5 && !bothOnHelpers; ++call) {
        ClaimRecorder::Reset();
        const TxError status = verify();
        BOOST_REQUIRE_MESSAGE(bsv::TxErrorIsOk(status), "parallel: " << status.domain << "/" << status.code);
        bothOnHelpers = ClaimRecorder::OnHelper(1) && ClaimRecorder::OnHelper(2);
    }
    BOOST_CHECK_MESSAGE(bothOnHelpers, "the deep inputs were not both verified on helpers in 5 calls");
}

// Sets the process-wide helper limit for a scope and restores the previous one.
class HelperLimitGuard {
public:
    explicit HelperLimitGuard(int64_t helpers) : previous_{ bsv::GetParallelInputHelpers() }
    {
        bsv::SetParallelInputHelpers(helpers);
    }
    ~HelperLimitGuard() { bsv::SetParallelInputHelpers(previous_); }
    HelperLimitGuard(const HelperLimitGuard&) = delete;
    HelperLimitGuard& operator=(const HelperLimitGuard&) = delete;

private:
    const int64_t previous_;
};

// Every input claimed inside a parallel job sleeps for `delay`; records the
// most helpers verifying an input at the same moment, process-wide. A helper
// is any thread other than the test's callers, which mark themselves with
// `isCaller`.
class HelperConcurrency {
public:
    explicit HelperConcurrency(std::chrono::milliseconds delay)
    {
        sleepMs.store(delay.count());
        Reset();
        previous = bsv::detail::SetInputHookForTesting(&HelperConcurrency::Hook);
    }
    ~HelperConcurrency() { bsv::detail::SetInputHookForTesting(previous); }
    HelperConcurrency(const HelperConcurrency&) = delete;
    HelperConcurrency& operator=(const HelperConcurrency&) = delete;

    static size_t Most() { return most.load(); }
    static size_t InFlight() { return inFlight.load(); }
    static uint64_t HelperClaims() { return helperClaims.load(); }
    static void Reset() { most.store(0); }
    static void SetDelay(std::chrono::milliseconds delay) { sleepMs.store(delay.count()); }
    static inline thread_local bool isCaller = false;

private:
    static void Hook(size_t, bsv::detail::InputEvent event)
    {
        if (event != bsv::detail::InputEvent::Claimed)
            return;
        const auto delay = std::chrono::milliseconds(sleepMs.load());
        if (isCaller) {
            std::this_thread::sleep_for(delay);
            return;
        }
        helperClaims.fetch_add(1);
        const size_t now = inFlight.fetch_add(1) + 1;
        size_t seen = most.load();
        while (now > seen && !most.compare_exchange_weak(seen, now)) {
        }
        std::this_thread::sleep_for(delay);
        inFlight.fetch_sub(1);
    }
    static inline std::atomic<long long> sleepMs{ 0 };
    static inline std::atomic<size_t> inFlight{ 0 };
    static inline std::atomic<size_t> most{ 0 };
    static inline std::atomic<uint64_t> helperClaims{ 0 };
    bsv::detail::InputHook previous = nullptr;
};

// Validates a 48-input tx of slow inputs on `callers` threads at once while
// the test thread frees a core five times (each release wakes a parked
// helper, as a caller leaving would), and returns the most helpers seen
// verifying at the same moment.
static size_t MostConcurrentHelpers(size_t callers)
{
    constexpr size_t nInputs = 48;
    const auto etxBin = SignedP2PKHBin(nInputs);
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    BOOST_REQUIRE(PoolIsIdle());
    const HelperConcurrency probe(std::chrono::milliseconds(10));
    std::atomic<size_t> failures{ 0 };
    std::atomic<size_t> finished{ 0 };
    std::vector<std::thread> threads;
    for (size_t t = 0; t < callers; ++t) {
        threads.emplace_back([&] {
            HelperConcurrency::isCaller = true;
            if (!bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)))
                failures.fetch_add(1);
            finished.fetch_add(1);
        });
    }
    for (int wake = 0; wake < 5 && finished.load() < callers; ++wake) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        bsv::detail::ScopedBusyThreadsForTesting oneCore(1);
        oneCore.Release();
    }
    for (auto& thread : threads)
        thread.join();
    BOOST_CHECK_EQUAL(failures.load(), 0u);
    BOOST_CHECK(PoolIsIdle());
    return HelperConcurrency::Most();
}

// The library ships with an automatic helper count; the setting reads back
// as set, every negative value reading back as -1.
BOOST_AUTO_TEST_CASE(helper_limit_defaults_to_automatic_and_reads_back)
{
    BOOST_CHECK_EQUAL(shippedParallelInputHelpers, -1);
    const HelperLimitGuard restore(bsv::GetParallelInputHelpers());
    for (const int64_t helpers : { int64_t{ 0 }, int64_t{ 1 }, int64_t{ 3 }, int64_t{ 1000 }, int64_t{ -1 } }) {
        bsv::SetParallelInputHelpers(helpers);
        BOOST_CHECK_EQUAL(bsv::GetParallelInputHelpers(), helpers);
    }
    for (const int64_t helpers : { int64_t{ -2 }, std::numeric_limits<int64_t>::min() }) {
        bsv::SetParallelInputHelpers(helpers);
        BOOST_CHECK_EQUAL(bsv::GetParallelInputHelpers(), -1);
    }
}

// With the limit at 0, a large tx is verified by its caller alone: no job is
// queued and no helper verifies an input, however idle the pool is.
BOOST_AUTO_TEST_CASE(no_helpers_verifies_every_tx_on_its_caller, PARALLEL_TEST_CASE_DECORATORS)
{
    if (!PoolHasHelpers())
        return;
    constexpr size_t nInputs = 200;
    const bsv::CTxValidator se("main");
    const auto etxBin = SignedP2PKHBin(nInputs);
    const InputThresholdGuard parallel(FORCE_PARALLEL);
    BOOST_REQUIRE(PoolIsIdle());
    const HelperLimitGuard none(0);
    const uint64_t jobInputs = bsv::detail::JobVerifiedInputCountForTesting();
    const uint64_t helperInputs = bsv::detail::HelperVerifiedInputCountForTesting();
    for (int call = 0; call < 5; ++call)
        BOOST_CHECK(bsv::TxErrorIsOk(ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true)));
    BOOST_CHECK_EQUAL(bsv::detail::JobVerifiedInputCountForTesting() - jobInputs, 0u);
    BOOST_CHECK_EQUAL(bsv::detail::HelperVerifiedInputCountForTesting() - helperInputs, 0u);
    BOOST_CHECK_EQUAL(bsv::detail::QueuedJobCountForTesting(), 0u);
}

// With the limit at n, no more than n helpers verify inputs at once,
// process-wide: not when two large txs run together, and not when freed cores
// wake parked helpers while a job runs. At least one helper still joins.
BOOST_AUTO_TEST_CASE(helper_limit_caps_concurrent_helpers, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers < 2) {
        BOOST_TEST_MESSAGE(helpers << " helper(s): a limit below the pool size needs at least 2");
        return;
    }
    for (const int64_t limit : { int64_t{ 1 }, int64_t{ 2 } }) {
        if (static_cast<size_t>(limit) >= helpers)
            continue;
        const HelperLimitGuard capped(limit);
        for (const size_t callers : { size_t{ 1 }, size_t{ 2 } }) {
            const size_t most = MostConcurrentHelpers(callers);
            BOOST_CHECK_MESSAGE(most >= 1 && most <= static_cast<size_t>(limit),
                "limit " << limit << ", " << callers << " caller(s): " << most << " helpers verified at once");
        }
    }
}

// Lowering the limit while a job runs sends the helpers over it away before
// their next input: lowered to 1, exactly one helper keeps verifying once the
// inputs in flight finish; lowered to 0, the helper still on the job
// claims no input after the one it is verifying. Inputs sleep 10 ms on claim.
BOOST_AUTO_TEST_CASE(lowering_the_helper_limit_sends_helpers_away_mid_job, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers < 2) {
        BOOST_TEST_MESSAGE(helpers << " helper(s): lowering from automatic to 1 needs at least 2");
        return;
    }
    constexpr size_t nInputs = 160;
    const auto etxBin = SignedP2PKHBin(nInputs);
    const bsv::CTxValidator se("main");
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    const HelperLimitGuard automatic(-1);
    BOOST_REQUIRE(PoolIsIdle());
    const HelperConcurrency probe(std::chrono::milliseconds(10));
    std::atomic<bool> done{ false };
    TxError status = bsv::TxErrorOk();
    std::thread caller([&] {
        HelperConcurrency::isCaller = true;
        status = ValidateP2PKHSpend(se, etxBin, nInputs, /*consensus=*/true);
        done.store(true);
    });
    const bool spread = WaitFor([&] { return HelperConcurrency::InFlight() >= 2 || done.load(); }) && !done.load();

    bsv::SetParallelInputHelpers(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    HelperConcurrency::Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    const size_t mostAtOne = HelperConcurrency::Most();

    bsv::SetParallelInputHelpers(0);
    const size_t onJobAtZero = bsv::detail::BusyThreadCountForTesting() - 1;
    const uint64_t claimsAtZero = HelperConcurrency::HelperClaims();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    const uint64_t claimsAfterZero = HelperConcurrency::HelperClaims() - claimsAtZero;
    const bool stillRunning = !done.load();

    HelperConcurrency::SetDelay(std::chrono::milliseconds(0));
    caller.join();
    BOOST_CHECK(bsv::TxErrorIsOk(status));
    BOOST_REQUIRE_MESSAGE(spread, "no two helpers verified at once before the limit was lowered");
    BOOST_REQUIRE_MESSAGE(stillRunning, "the job finished before the checks");
    BOOST_CHECK_MESSAGE(mostAtOne == 1, mostAtOne << " helpers verified at once after the limit was lowered to 1");
    BOOST_CHECK_MESSAGE(claimsAfterZero <= onJobAtZero, claimsAfterZero << " helper claims after the limit was lowered "
        "to 0, with " << onJobAtZero << " helper(s) on the job");
    BOOST_CHECK(PoolIsIdle());
}

// The automatic setting (any negative value) uses more than one helper when
// the pool has them: a single caller's job gets several within a few calls.
BOOST_AUTO_TEST_CASE(automatic_helper_limit_uses_the_pool, PARALLEL_TEST_CASE_DECORATORS)
{
    const size_t helpers = bsv::detail::InputPoolHelperCountForTesting();
    if (helpers < 2) {
        BOOST_TEST_MESSAGE(helpers << " helper(s): nothing to tell apart from a limit of 1");
        return;
    }
    for (const int64_t automatic : { int64_t{ -1 }, int64_t{ -5 } }) {
        const HelperLimitGuard limit(automatic);
        size_t most = 0;
        for (int call = 0; call < 5 && most < 2; ++call)
            most = std::max(most, MostConcurrentHelpers(1));
        BOOST_CHECK_MESSAGE(most >= 2, "setting " << automatic << ": at most " << most << " helper(s) at once");
    }
}

// Results do not depend on the helper limit, and changing it while callers
// validate is safe: each tx returns its serial result at every setting, then
// while another thread keeps switching the limit between automatic, 0, 1, 2
// and more than the pool has.
BOOST_AUTO_TEST_CASE(results_hold_at_every_helper_limit_and_while_it_changes, PARALLEL_TEST_CASE_DECORATORS)
{
    const bsv::CTxValidator se("main");
    const SigHashType allForkId = SigHashType().withForkId();
    constexpr size_t nInputs = 60;
    const auto spend = [&] {
        return MakeSignedP2PKHSpend(std::vector<SigHashType>(nInputs, allForkId), 2, /*precomputeForSigning=*/true);
    };
    std::vector<bsv::CMutableTransactionExtended> txs;
    txs.push_back(spend());                    // valid
    txs.push_back(spend());
    CorruptP2PKHSignature(txs.back(), 7);      // NULLFAIL
    txs.push_back(spend());
    EmptyP2PKHSignature(txs.back(), 40);       // EVAL_FALSE, above it NULLFAIL
    CorruptP2PKHSignature(txs.back(), 50);
    {
        std::vector<SigHashType> types(nInputs, allForkId);
        types[25] = SigHashType();             // MUST_USE_FORKID
        txs.push_back(MakeSignedP2PKHSpend(types, 2, /*precomputeForSigning=*/true));
    }
    std::vector<std::vector<uint8_t>> bins;
    const std::vector<int32_t> heights(nInputs, P2PKH_UTXO_HEIGHT);
    std::vector<TxError> expected;
    for (const auto& tx : txs) {
        bins.push_back(SerializeExtended(tx));
        expected.push_back(FirstFailingSpend(se, tx, heights, /*consensus=*/true));
    }
    const auto validate = [&](size_t i) {
        return se.ValidateTransaction(std::span<const uint8_t>(bins[i]), std::span<const int32_t>(heights),
            P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
    };

    const InputThresholdGuard parallel(FORCE_PARALLEL);
    const HelperLimitGuard restore(bsv::GetParallelInputHelpers());
    constexpr std::array<int64_t, 5> SETTINGS{ -1, 0, 1, 2, 1000 };
    for (const int64_t helpers : SETTINGS) {
        bsv::SetParallelInputHelpers(helpers);
        for (size_t i = 0; i < txs.size(); ++i) {
            const TxError status = validate(i);
            BOOST_CHECK_MESSAGE(SameTxError(status, expected[i]), "helpers " << helpers << ", tx " << i
                << ": domain=" << status.domain << " code=" << status.code);
        }
    }

    const size_t callerCount = std::max<size_t>(2, (bsv::detail::InputPoolHelperCountForTesting() + 1) / 2);
    std::atomic<bool> stop{ false };
    std::atomic<size_t> mismatches{ 0 };
    std::atomic<size_t> calls{ 0 };
    std::thread switcher([&] {
        for (size_t s = 0; !stop.load(); ++s) {
            bsv::SetParallelInputHelpers(SETTINGS[s % SETTINGS.size()]);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });
    std::vector<std::thread> callers;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    for (size_t t = 0; t < callerCount; ++t) {
        callers.emplace_back([&, t] {
            for (size_t r = t; std::chrono::steady_clock::now() < until; ++r) {
                const size_t i = r % txs.size();
                if (!SameTxError(validate(i), expected[i]))
                    mismatches.fetch_add(1);
                calls.fetch_add(1);
            }
        });
    }
    for (auto& caller : callers)
        caller.join();
    stop.store(true);
    switcher.join();
    BOOST_CHECK_EQUAL(mismatches.load(), 0u);
    BOOST_CHECK_GT(calls.load(), 0u);
    BOOST_CHECK(PoolIsIdle());
    BOOST_CHECK_EQUAL(bsv::detail::QueuedJobCountForTesting(), 0u);
}

#undef PARALLEL_TEST_CASE_DECORATORS

BOOST_AUTO_TEST_SUITE_END()

// The helper pool sizes itself from the CPUs this process may use. On Linux a
// container's CPU limit comes from its cgroup; these tests read fixture trees
// under test/core/data/cgroup, each laid out as a root with proc/self/cgroup,
// proc/self/mountinfo and the cgroup files, never the live system.
BOOST_AUTO_TEST_SUITE(container_cpu_limit)

static std::optional<double> FixtureCpuLimit(const std::string& name)
{
    return bsv::detail::CgroupCpuLimit(std::string(BDK_CGROUP_FIXTURE_DIR) + "/" + name);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_quota_in_a_container)
{
    const auto limit = FixtureCpuLimit("v2_quota");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_quota_of_a_nested_cgroup)
{
    // The process's own cgroup has the limit; the root's cpu.max says max.
    const auto limit = FixtureCpuLimit("v2_nested");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 1.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_max_is_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v2_max").has_value());
}

BOOST_AUTO_TEST_CASE(cgroup_v1_quota_in_a_container)
{
    const auto limit = FixtureCpuLimit("v1_quota");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v1_quota_minus_one_is_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v1_unlimited").has_value());
}

// A v1 cpuset hierarchy listed before cpu,cpuacct: only the hierarchy whose
// controller list contains exactly "cpu" counts (the cpuset mount holds a
// decoy 4-CPU quota).
BOOST_AUTO_TEST_CASE(cgroup_v1_cpu_controller_is_matched_exactly)
{
    const auto limit = FixtureCpuLimit("v1_cpuset_first");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.0, 1e-9);
}

// mountinfo escapes a space in the mount point as \040.
BOOST_AUTO_TEST_CASE(cgroup_mount_point_is_unescaped)
{
    const auto limit = FixtureCpuLimit("v2_escaped_mount");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 1.2, 1e-9);
}

// A cgroup shown as /../.. lies outside the mount this namespace sees; the
// path it would resolve to holds a decoy limit.
BOOST_AUTO_TEST_CASE(cgroup_outside_the_namespace_gives_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v2_outside_namespace").has_value());
}

// A mount rooted at /kubepods/podA does not contain /kubepods/podAB/ctr; the
// next mount, rooted at /, does (the wrong one would read a decoy 0.5).
BOOST_AUTO_TEST_CASE(cgroup_mount_root_matches_whole_path_components)
{
    const auto limit = FixtureCpuLimit("v2_prefix_not_boundary");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 3.0, 1e-9);
}

// A malformed mountinfo line (no " - " separator, or too few fields after
// it), a mount root not starting with /, or a bad \ooo escape in a mount
// point makes the file unreadable even when a valid cgroup2 mount follows,
// as in Go. Where the malformed line would otherwise resolve to a cgroup
// directory, that directory holds a decoy limit.
BOOST_AUTO_TEST_CASE(malformed_mountinfo_gives_no_limit)
{
    for (const char* name : { "malformed_mount_line", "malformed_mount_root", "malformed_mount_tail", "malformed_mount_escape" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

BOOST_AUTO_TEST_CASE(unreadable_cgroup_files_give_no_limit)
{
    for (const char* name : { "garbage", "garbage_cgroup", "missing", "missing_cpu_max", "no_such_fixture" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

// Go's default GOMAXPROCS rule (runtime/cgroup_linux.go, the same from go1.25.0 on,
// adjustCgroupGOMAXPROCS): the CPUs available to the process, lowered to the
// cgroup limit rounded up, and never below 2 because of the limit.
// On this machine, whatever its cgroup: at least one CPU, never more than
// hardware_concurrency(). On Linux this reads the live affinity mask and cgroup.
BOOST_AUTO_TEST_CASE(effective_cpu_count_is_within_the_machine)
{
    const size_t effective = bsv::detail::EffectiveCpuCount();
    const size_t hardware = std::thread::hardware_concurrency();
    const auto limit = bsv::detail::CgroupCpuLimit("/");
    BOOST_TEST_MESSAGE("effective=" << effective << " hardware_concurrency=" << hardware
        << " cgroup_limit=" << (limit ? std::to_string(*limit) : std::string("none")));
    BOOST_CHECK_GE(effective, 1u);
    if (hardware > 0)
        BOOST_CHECK_LE(effective, hardware);
}

#if defined(__linux__)
static std::string FixtureRoot(const std::string& name)
{
    return std::string(BDK_CGROUP_FIXTURE_DIR) + "/" + name;
}

// EffectiveCpuCount lowers the count to the cgroup limit under the root it is
// given. A machine with more than 3 CPUs shows it: 2.5 CPUs round up to 3.
BOOST_AUTO_TEST_CASE(effective_cpu_count_applies_the_cgroup_limit)
{
    const size_t unlimited = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max"));
    const size_t limited = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_quota"));
    BOOST_CHECK_EQUAL(limited, bsv::detail::LimitCpuCount(unlimited, 2.5));
    if (unlimited > 3)
        BOOST_CHECK_EQUAL(limited, 3u);
    else
        BOOST_TEST_MESSAGE("only " << unlimited << " CPUs: the 2.5-CPU limit cannot lower the count here");
}

// EffectiveCpuCount respects the calling thread's affinity mask, which
// hardware_concurrency() ignores. The pool is created first, so pinning this
// thread cannot size it.
BOOST_AUTO_TEST_CASE(effective_cpu_count_applies_the_affinity_mask)
{
    (void)bsv::detail::InputPoolHelperCountForTesting();
    cpu_set_t original;
    CPU_ZERO(&original);
    BOOST_REQUIRE_EQUAL(sched_getaffinity(0, sizeof(original), &original), 0);
    int pinned = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE && pinned < 0; ++cpu)
        if (CPU_ISSET(cpu, &original))
            pinned = cpu;
    BOOST_REQUIRE_GE(pinned, 0);
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(pinned, &one);
    struct RestoreAffinity {
        const cpu_set_t& mask;
        ~RestoreAffinity() { sched_setaffinity(0, sizeof(mask), &mask); }
    } restore{ original };
    BOOST_REQUIRE_EQUAL(sched_setaffinity(0, sizeof(one), &one), 0);
    BOOST_CHECK_EQUAL(bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max")), 1u);
}
#endif

BOOST_AUTO_TEST_CASE(cpu_count_rounds_the_limit_like_gomaxprocs)
{
    using bsv::detail::LimitCpuCount;
    BOOST_CHECK_EQUAL(LimitCpuCount(16, std::nullopt), 16u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 4.0), 4u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 2.5), 3u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 1.01), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 0.5), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(4, 8.0), 4u);
    BOOST_CHECK_EQUAL(LimitCpuCount(1, 0.5), 1u);
}

BOOST_AUTO_TEST_SUITE_END()

// Throughput numbers for choosing and checking the parallel input threshold,
// about 45 s. Disabled by default and not registered with ctest; run it alone
// on an otherwise idle machine:
//   test_txvalidator --run_test=test_txvalidator/parallel_input_benchmark
// CI runs it in an optional step (workflow_dispatch input run-benchmarks).
// It asserts only that every tx validates.
BOOST_AUTO_TEST_SUITE(parallel_input_benchmark, *boost::unit_test::disabled())

using BenchClock = std::chrono::steady_clock;

static double ElapsedUs(BenchClock::time_point t0)
{
    return std::chrono::duration<double, std::micro>(BenchClock::now() - t0).count();
}

struct BenchTx {
    std::vector<uint8_t> bin;
    std::vector<int32_t> heights;
    size_t inputs;
};

static BenchTx MakeBenchTx(size_t nInputs)
{
    const auto eTx = MakeSignedP2PKHSpend(std::vector<SigHashType>(nInputs, SigHashType().withForkId()),
        /*nOutputs=*/1, /*precomputeForSigning=*/true);
    return { SerializeExtended(eTx), std::vector<int32_t>(nInputs, P2PKH_UTXO_HEIGHT), nInputs };
}

static TxError ValidateBenchTx(const bsv::CTxValidator& se, const BenchTx& tx)
{
    return se.ValidateTransaction(std::span<const uint8_t>(tx.bin), std::span<const int32_t>(tx.heights),
        P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
}

// Best wall-clock time of one ValidateTransaction call, in microseconds.
// Each sample repeats the call until it spans at least 20 ms.
static double BestUsPerTx(const bsv::CTxValidator& se, const BenchTx& tx, size_t threshold, int samples)
{
    const InputThresholdGuard guard(threshold);
    BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateBenchTx(se, tx)));
    double best = std::numeric_limits<double>::max();
    for (int s = 0; s < samples; ++s) {
        size_t calls = 0;
        const auto t0 = BenchClock::now();
        double us = 0;
        do {
            BOOST_REQUIRE(bsv::TxErrorIsOk(ValidateBenchTx(se, tx)));
            ++calls;
            us = ElapsedUs(t0);
        } while (us < 20000.0);
        best = std::min(best, us / static_cast<double>(calls));
    }
    return best;
}

// Serial vs parallel at small input counts. The parallel run sets the threshold
// to the tx's own input count, so it measures what a tx right at that threshold
// would get.
BOOST_AUTO_TEST_CASE(threshold_sweep)
{
    const bsv::detail::ScopedInputCountersForTesting productionCounters(false);
    const bsv::CTxValidator se("main");
    std::cerr << "[threshold_sweep] hardware_concurrency=" << std::thread::hardware_concurrency() << std::endl;
    std::cerr << "[threshold_sweep] inputs serial_us parallel_us speedup" << std::endl;
    for (const size_t n : { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 256, 512, 1024 }) {
        const BenchTx tx = MakeBenchTx(n);
        const double parallel = BestUsPerTx(se, tx, n, 7);
        const double serial = BestUsPerTx(se, tx, FORCE_SERIAL, 7);
        std::cerr << "[threshold_sweep] " << n << " " << serial << " " << parallel << " " << serial / parallel << std::endl;
    }
}

// ValidateTransaction at 1k, 10k and 40k SIGHASH_ALL|FORKID P2PKH inputs.
BOOST_AUTO_TEST_CASE(large_tx_serial_vs_parallel)
{
    const bsv::detail::ScopedInputCountersForTesting productionCounters(false);
    const bsv::CTxValidator se("main");
    const size_t defaultThreshold = bsv::detail::SetParallelInputThresholdForTesting(FORCE_SERIAL);
    bsv::detail::SetParallelInputThresholdForTesting(defaultThreshold);
    std::cerr << "[large_tx_serial_vs_parallel] hardware_concurrency=" << std::thread::hardware_concurrency()
              << " default_threshold=" << defaultThreshold << std::endl;
    std::cerr << "[large_tx_serial_vs_parallel] inputs serial_ms parallel_ms speedup" << std::endl;
    for (const size_t n : { 1000, 10000, 40000 }) {
        const BenchTx tx = MakeBenchTx(n);
        const double serial = BestUsPerTx(se, tx, FORCE_SERIAL, 3) / 1000.0;
        const double parallel = BestUsPerTx(se, tx, defaultThreshold, 3) / 1000.0;
        std::cerr << "[large_tx_serial_vs_parallel] " << n << " " << serial << " " << parallel << " "
                  << serial / parallel << std::endl;
    }
}

// Aggregate inputs/s with many callers at once, parallel inputs off (serial
// threshold) and on (default threshold). Each caller validates its txs in a
// loop for a fixed time.
static double AggregateInputsPerSecond(const bsv::CTxValidator& se, const std::vector<const BenchTx*>& perCaller,
                                       size_t threshold)
{
    const InputThresholdGuard guard(threshold);
    constexpr auto DURATION = std::chrono::milliseconds(1000);
    std::atomic<uint64_t> inputs{ 0 };
    std::atomic<size_t> failures{ 0 };
    std::atomic<bool> start{ false };
    std::vector<std::thread> callers;
    for (const BenchTx* tx : perCaller) {
        callers.emplace_back([&, tx] {
            while (!start.load())
                std::this_thread::yield();
            const auto stop = BenchClock::now() + DURATION;
            uint64_t mine = 0;
            while (BenchClock::now() < stop) {
                if (!bsv::TxErrorIsOk(ValidateBenchTx(se, *tx)))
                    failures.fetch_add(1);
                mine += tx->inputs;
            }
            inputs.fetch_add(mine);
        });
    }
    const auto t0 = BenchClock::now();
    start.store(true);
    for (auto& caller : callers)
        caller.join();
    BOOST_REQUIRE_EQUAL(failures.load(), 0u);
    return static_cast<double>(inputs.load()) / (ElapsedUs(t0) / 1e6);
}

BOOST_AUTO_TEST_CASE(concurrent_callers_throughput)
{
    const bsv::detail::ScopedInputCountersForTesting productionCounters(false);
    const bsv::CTxValidator se("main");
    const size_t cores = std::max<size_t>(1, std::thread::hardware_concurrency());
    const size_t defaultThreshold = bsv::detail::SetParallelInputThresholdForTesting(FORCE_SERIAL);
    bsv::detail::SetParallelInputThresholdForTesting(defaultThreshold);
    const BenchTx small = MakeBenchTx(2);
    const BenchTx medium = MakeBenchTx(1000);
    const BenchTx large = MakeBenchTx(10000);

    struct Scenario { std::string name; std::vector<const BenchTx*> perCaller; };
    std::vector<Scenario> scenarios;
    scenarios.push_back({ "cores x 2-input", std::vector<const BenchTx*>(cores, &small) });
    scenarios.push_back({ "cores x 1000-input", std::vector<const BenchTx*>(cores, &medium) });
    scenarios.push_back({ "2*cores x 1000-input", std::vector<const BenchTx*>(2 * cores, &medium) });
    std::vector<const BenchTx*> mixed(cores - (cores > 1 ? 1 : 0), &small);
    mixed.push_back(&large);
    scenarios.push_back({ "(cores-1) x 2-input + 1 x 10000-input", mixed });
    scenarios.push_back({ "1 x 10000-input", std::vector<const BenchTx*>(1, &large) });

    std::cerr << "[concurrent_callers_throughput] hardware_concurrency=" << cores
              << " default_threshold=" << defaultThreshold << std::endl;
    std::cerr << "[concurrent_callers_throughput] scenario | serial_inputs_per_s | parallel_inputs_per_s | ratio" << std::endl;
    for (const Scenario& s : scenarios) {
        // Alternating order, best of 3 each, so clock and thermal drift don't favour either.
        double serial = 0;
        double parallel = 0;
        for (int round = 0; round < 3; ++round) {
            const bool serialFirst = (round % 2) == 0;
            if (serialFirst)
                serial = std::max(serial, AggregateInputsPerSecond(se, s.perCaller, FORCE_SERIAL));
            parallel = std::max(parallel, AggregateInputsPerSecond(se, s.perCaller, defaultThreshold));
            if (!serialFirst)
                serial = std::max(serial, AggregateInputsPerSecond(se, s.perCaller, FORCE_SERIAL));
        }
        std::cerr << "[concurrent_callers_throughput] " << s.name << " | " << serial << " | " << parallel
                  << " | " << parallel / serial << std::endl;
    }
}

// The core budget under concurrent callers: C callers validate a 10k-input tx
// in a loop for one second while a monitor samples the busy count every ~50 us.
// Prints the peak and how often it exceeded max(cores, C), the most the callers
// alone account for. Run with -- --input-pool-cores=N to size the pool.
BOOST_AUTO_TEST_CASE(core_budget_under_concurrent_callers)
{
    const bsv::detail::ScopedInputCountersForTesting productionCounters(false);
    const size_t cores = bsv::detail::InputPoolHelperCountForTesting() + 1;
    const bsv::CTxValidator se("main");
    const BenchTx large = MakeBenchTx(10000);
    const InputThresholdGuard defaults(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD);
    std::cerr << "[core_budget_under_concurrent_callers] pool_cores=" << cores << std::endl;
    std::cerr << "[core_budget_under_concurrent_callers] callers | peak_busy | samples_over_max(cores,callers) | samples_over_cores" << std::endl;
    for (const size_t callerCount : { size_t{ 1 }, size_t{ 2 }, size_t{ 3 }, cores, 2 * cores }) {
        std::atomic<bool> stop{ false };
        std::atomic<size_t> failures{ 0 };
        std::vector<std::thread> callers;
        for (size_t t = 0; t < callerCount; ++t) {
            callers.emplace_back([&] {
                while (!stop.load())
                    if (!bsv::TxErrorIsOk(ValidateBenchTx(se, large)))
                        failures.fetch_add(1);
            });
        }
        size_t peak = 0, samples = 0, overCallers = 0, overCores = 0;
        const size_t allowed = std::max(cores, callerCount);
        const auto end = BenchClock::now() + std::chrono::seconds(1);
        while (BenchClock::now() < end) {
            const size_t busy = bsv::detail::BusyThreadCountForTesting();
            peak = std::max(peak, busy);
            ++samples;
            overCallers += busy > allowed;
            overCores += busy > cores;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        stop.store(true);
        for (auto& caller : callers)
            caller.join();
        BOOST_REQUIRE_EQUAL(failures.load(), 0u);
        std::cerr << "[core_budget_under_concurrent_callers] " << callerCount << " | " << peak << " | "
                  << overCallers << "/" << samples << " | " << overCores << "/" << samples << std::endl;
    }
}

// Peak stack used to verify one input, measured by painting a 2 MiB region
// below this frame, verifying the input with VerifySpend on this thread, and
// counting how much of the pattern was overwritten. A measurement only: it
// reads the painted bytes back through a fresh frame at the same depth.
constexpr size_t STACK_PROBE_BYTES = size_t{ 2 } << 20;

[[gnu::noinline]] static void PaintStack()
{
    volatile unsigned char area[STACK_PROBE_BYTES];
    for (size_t i = 0; i < STACK_PROBE_BYTES; ++i)
        area[i] = 0xa5;
}

[[gnu::noinline]] static size_t UnusedStackBytes()
{
    volatile unsigned char area[STACK_PROBE_BYTES];
    size_t untouched = 0;
    while (untouched < STACK_PROBE_BYTES && area[untouched] == 0xa5)
        ++untouched;
    return untouched;
}

// Calibration for the probe: a frame that touches `Bytes` of stack.
template <size_t Bytes>
[[gnu::noinline]] static void UseStack()
{
    volatile unsigned char area[Bytes];
    for (size_t i = 0; i < Bytes; ++i)
        area[i] = 0;
}

BOOST_AUTO_TEST_CASE(stack_use_of_the_deepest_scripts)
{
    const bsv::detail::ScopedInputCountersForTesting productionCounters(false);
    const bsv::CTxValidator se("main");
    const auto eTx = parallel_input_verification::MakeDeepScriptSpend(
        parallel_input_verification::DEEP_IF_DEPTH, parallel_input_verification::DEEP_MULTISIG_KEYS);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx.mtx;
    const std::vector<uint8_t> txBin(ss.begin(), ss.end());
    std::cerr << "[stack_use_of_the_deepest_scripts] input | peak_stack_bytes (probe " << STACK_PROBE_BYTES << ")" << std::endl;
    PaintStack();
    UseStack<65536>();
    std::cerr << "[stack_use_of_the_deepest_scripts] calibration, a 65536-byte frame | "
              << STACK_PROBE_BYTES - UnusedStackBytes() << std::endl;
    for (const auto& [index, name] : { std::pair{ size_t{ 0 }, "P2PKH" }, std::pair{ size_t{ 1 }, "OP_IF nested 100000 deep" },
                                       std::pair{ size_t{ 2 }, "1-of-1000 multisig, 1000 checks" } }) {
        const CTxOut& source = eTx.vutxo[index];
        PaintStack();
        const TxError status = se.VerifySpend(std::span<const uint8_t>(txBin), static_cast<uint32_t>(index),
            std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
            source.nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
        const size_t used = STACK_PROBE_BYTES - UnusedStackBytes();
        BOOST_REQUIRE(bsv::TxErrorIsOk(status));
        std::cerr << "[stack_use_of_the_deepest_scripts] " << name << " | " << used << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
