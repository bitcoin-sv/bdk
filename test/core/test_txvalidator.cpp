#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
#include "sha256_backend.hpp"
#include "crypto/sha256_dispatch.h"
#include "sha256_backend_expectations.h"
#include "chainparams_bdk.hpp"
#include "extendedTx.hpp"
#include "txerror.h"

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_txvalidator)

// A custom chain params
class MyParams : public CChainParams {
public:
    static constexpr const char* CustomName = "MyParams";

    MyParams() = default;
};


// Registration custom chain params
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): registered before main() by design
const static bsv::RegisterCustomChainParams<MyParams> myParamsReg(MyParams::CustomName);

// BDK installs the fastest SHA-256 transforms this CPU supports while it is
// loaded, so every validator, and every test below, validates through them.
// These cases check the installed pointers after each constructor, before the
// getter could install them itself. They do not check when selection happened;
// the race test and Go/Rust at-load tests cover load-time selection. ctest also
// runs each case alone, in its own process.
BOOST_AUTO_TEST_CASE(validator_by_network_has_sha256_backend)
{
    const bsv::CTxValidator validator{bsv::TxValidationNetwork::Main};
    sha256_test::CheckInstalledBackend();
    BOOST_TEST(bsv::SelectSha256Backend() == sha256_test::ExpectedSelection());
    std::cout << "SHA-256 backend: " << bsv::SelectSha256Backend() << '\n';
}

// The same through the chain-name constructor, which ctest also runs alone, in
// its own process (test_txvalidator_chain_name_has_backend).
BOOST_AUTO_TEST_CASE(validator_by_chain_name_has_sha256_backend)
{
    const bsv::CTxValidator validator{std::string{"main"}};
    sha256_test::CheckInstalledBackend();
    BOOST_TEST(bsv::SelectSha256Backend() == sha256_test::ExpectedSelection());
}

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
    const std::span<const int32_t> utxo(utxoArray);
    const bsv::CTxValidator se("main");
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
    const std::span<const int32_t> utxo(utxoArray);
    const std::span<const uint32_t> customFlags(customFlagsArray);
    const bsv::CTxValidator se("main");
    const auto status = se.VerifyScript(etx, utxo, blockHeight, true, customFlags);  // VerifyScript with custom flags stays as-is
    BOOST_CHECK(bsv::TxErrorIsOk(status));
}

BOOST_AUTO_TEST_CASE(test_verify_empty_utxos)
{
    // A extended transaction with zero input is expected to failed
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the beta-8
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    const std::span<const int32_t> utxo;
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

    const bsv::CTxValidator se("main");
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
              << " elapsed_ms=" << elapsed << '\n';

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
        const bsv::CTxValidator se("main");
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

    const auto runOnce = [&](uint64_t minConsolidationFactor) {
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

    const bsv::CTxValidator se("main");
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

    const bsv::CTxValidator se("main");
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
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
    opcodetype opcode{};
    std::vector<uint8_t> sig;
    std::vector<uint8_t> pubkey;
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, sig));
    BOOST_REQUIRE(eTx.mtx.vin[index].scriptSig.GetOp(pc, opcode, pubkey));
    BOOST_REQUIRE(sig.size() > 5 && sig[0] == 0x30 && sig[2] == 0x02);
    const size_t rLen = sig[3];
    sig[4 + rLen - 1] ^= 0x01;
    eTx.mtx.vin[index].scriptSig = CScript() << sig << pubkey;
}

static const std::array<SigHashType, 6>& ForkIdSigHashTypes()
{
    static const std::array<SigHashType, 6> types{
    SigHashType().withBaseType(BaseSigHashType::ALL).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::NONE).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId(),
    SigHashType().withBaseType(BaseSigHashType::ALL).withForkId().withAnyoneCanPay(),
    SigHashType().withBaseType(BaseSigHashType::NONE).withForkId().withAnyoneCanPay(),
    SigHashType().withBaseType(BaseSigHashType::SINGLE).withForkId().withAnyoneCanPay(),
    };
    return types;
}

// Each FORKID sighash type, with and without ANYONECANPAY, on every input of a
// multi-input, multi-output tx. SINGLE has a matching output for every input.
BOOST_AUTO_TEST_CASE(forkid_sighash_types_validate_on_multi_input_tx)
{
    const bsv::CTxValidator se("main");
    for (const SigHashType& type : ForkIdSigHashTypes()) {
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
    const std::vector<SigHashType> types(ForkIdSigHashTypes().begin(), ForkIdSigHashTypes().end());
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
    spend.eTx.mtx.vin[index].scriptSig = std::move(scriptSig);
    return spend;
}

// Flips the low bit of the last byte of r in the second multisig signature,
// keeping it strict-DER and low-S.
static void CorruptSecondMultisigSignature(MultisigSpend& spend)
{
    const CScript& scriptSig = spend.eTx.mtx.vin[spend.index].scriptSig;
    CScript::const_iterator pc = scriptSig.begin();
    opcodetype opcode{};
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
                                 // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
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
    mtx.vin[index].scriptSig = std::move(scriptSig);

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
              << largeMarginalMs << " ratio=" << ratio << " max_ratio=" << MAX_RATIO << '\n';

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
              << " ratio=" << ratio << " max_ratio=" << MAX_RATIO << '\n';

    BOOST_CHECK_MESSAGE(ratio < MAX_RATIO,
        "ValidateTransaction CPU time ratio " << ratio << " for " << LARGE_N / SMALL_N
        << "x the inputs exceeds " << MAX_RATIO << ": sighash work is growing with the square of the input count");
}

BOOST_AUTO_TEST_SUITE_END()
