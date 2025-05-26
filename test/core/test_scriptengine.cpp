#include <memory>
#include <string>
#include <vector>

/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_scriptengine
#else
#define BOOST_TEST_MODULE test_scriptengined
#endif

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>

#include "streams.h"
#include "version.h"
#include "script/script.h"
#include "script/script_flags.h"
#include "utilstrencodings.h"
#include "verify_script_flags.h"
#include "consensus/params.h"
#include "protocol_era.h"
#include "chainparams.h"
#include "chainparamsbase.h"

#include "scriptengine.hpp"
#include "chainparams_bdk.hpp"
#include "extendedTx.hpp"

namespace ba = boost::algorithm;

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_scriptengine)

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

BOOST_AUTO_TEST_CASE(custom_genesis_height)
{
    const int32_t h{ 1000 };
    bsv::CScriptEngine se("main");
    std::string err;
    const bool ok = se.SetGenesisActivationHeight(h, &err);
    BOOST_CHECK(ok);
    BOOST_CHECK(err.empty());
    const auto gh = se.GetGenesisActivationHeight();
    BOOST_CHECK(h == gh);
}

BOOST_AUTO_TEST_CASE(test_check_consensus)
{
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the beta-8
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    const int32_t blockHeight = 632099;

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    std::span<const int32_t> utxo(utxoArray);
    bsv::CScriptEngine se("main");
    const auto rconsensus = se.CheckConsensus(etx, utxo, blockHeight);
    BOOST_CHECK_EQUAL(bitcoinconsensus_ERR_OK, rconsensus);
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
    bsv::CScriptEngine se("main");
    const auto status = se.VerifyScript(etx, utxo, blockHeight, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
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

    bsv::CScriptEngine se("main");
    const auto status = se.VerifyScript(emptyUtxoEtx, utxo, blockHeight, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_UNKNOWN_ERROR, status);
}

BOOST_AUTO_TEST_SUITE_END()
