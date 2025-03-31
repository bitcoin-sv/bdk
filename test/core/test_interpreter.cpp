#include <memory>
#include <string>
#include <vector>

/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_interpreter
#else
#define BOOST_TEST_MODULE test_interpreterd
#endif

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>

#include "streams.h"
#include "version.h"
#include "script/script.h"
#include "script/script_flags.h"
#include "utilstrencodings.h"

#include "interpreter_bdk.hpp"
#include "extendedTx.hpp"

namespace ba = boost::algorithm;

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_interpreter)

BOOST_AUTO_TEST_CASE(custom_chainparams)
{
    // Set global config with 'regular' chain should not throw an error
    BOOST_TEST(SetGlobalScriptConfig("main", 0, 0, 0, 0, 0, 0).empty());
    BOOST_TEST(SetGlobalScriptConfig("test", 0, 0, 0, 0, 0, 0).empty());
    BOOST_TEST(SetGlobalScriptConfig("regtest", 0, 0, 0, 0, 0, 0).empty());
    BOOST_TEST(SetGlobalScriptConfig("stn", 0, 0, 0, 0, 0, 0).empty());

    // Set global config with custom chain should not throw an error
    BOOST_TEST(SetGlobalScriptConfig("teratestnet", 0, 0, 0, 0, 0, 0).empty());
    BOOST_TEST(SetGlobalScriptConfig("tstn", 0, 0, 0, 0, 0, 0).empty());

    // Set global config with unknown chain should throw an exeption
    BOOST_TEST(SetGlobalScriptConfig("foo", 0, 0, 0, 0, 0, 0).find("CGO EXCEPTION") != std::string::npos);

    // Reset to main chain params for other tests (just in case)
    BOOST_TEST(SetGlobalScriptConfig("main", 0, 0, 0, 0, 0, 0).empty());
}

BOOST_AUTO_TEST_CASE(custom_genesis_height)
{
    const int32_t customGH = 123;
    SetGlobalScriptConfig("main", 0, 0, 0, 0, 0, 0); // Set genesis height to default
    const auto defaultGH = bsv::GetGenesisActivationHeight();
    BOOST_CHECK(defaultGH != customGH);
    SetGlobalScriptConfig("main", 0, 0, 0, 0, 0, 0, customGH);
    const auto setGH = bsv::GetGenesisActivationHeight();
    BOOST_CHECK_EQUAL(customGH, setGH);
}

BOOST_AUTO_TEST_CASE(pure_scripts)
{
    using test_data_type = tuple<vector<uint8_t>, ScriptError>;
    vector<test_data_type> test_data{
        {{OP_1, OP_1, OP_ADD, OP_2, OP_EQUALVERIFY}, SCRIPT_ERR_OK},
        {{OP_1, OP_1, OP_ADD, OP_3, OP_EQUALVERIFY}, SCRIPT_ERR_EQUALVERIFY},
    };
    for(const auto& [script, exp_status] : test_data)
    {
        constexpr bool consensus{true};
        constexpr auto flags{0};
        const auto status{execute(script, consensus, flags)};
        BOOST_CHECK_EQUAL(exp_status, status);
    }
}

BOOST_AUTO_TEST_CASE(empty_script)
{
    vector<uint8_t> script;
    constexpr bool consensus{true};
    constexpr auto flags{0};
    const auto expected{"script empty"};
    try
    {
        bsv::execute(script, consensus, flags);
        BOOST_CHECK(false);
    }
    catch(const std::runtime_error& e)
    {
        const auto actual{e.what()};
        BOOST_CHECK_EQUAL(expected, actual);
    }

    std::vector<uint8_t> tx;
    constexpr auto amt{10};
    constexpr auto index{0};
    try
    {
        bsv::execute(script, consensus, flags, tx, index, amt);
        BOOST_CHECK(false);
    }
    catch(const std::runtime_error& e)
    {
        const auto actual{e.what()};
        BOOST_CHECK_EQUAL(expected, actual);
    }

    try
    {
        bsv::execute(string{}, consensus, flags, string{}, index, amt);
        BOOST_CHECK(false);
    }
    catch(const std::runtime_error& e)
    {
        const auto actual{e.what()};
        BOOST_CHECK_EQUAL(expected, actual);
    }
}

BOOST_AUTO_TEST_CASE(empty_tx)
{
    vector<uint8_t> script{OP_ADD};
    constexpr bool consensus{true};
    constexpr auto flags{0};
    std::vector<uint8_t> tx;
    constexpr auto index{0};
    constexpr auto amt{10};

    const auto expected{"tx empty"};
    try
    {
        bsv::execute(script, consensus, flags, tx, index, amt);
        BOOST_CHECK(false);
    }
    catch(const std::runtime_error& e)
    {
        const auto actual{e.what()};
        BOOST_CHECK_EQUAL(expected, actual);
    }

    try
    {
        bsv::execute(string{"ADD"}, consensus, flags, string{}, index, amt);
        BOOST_CHECK(false);
    }
    catch(const std::runtime_error& e)
    {
        const auto actual{e.what()};
        BOOST_CHECK_EQUAL(expected, actual);
    }
}

BOOST_AUTO_TEST_CASE(test_case_1)
{
    // 'abc...', OP_HASH256, OP_PUSHDATA1, 20, '20 data bytes', OP_EQUALVERIFY
    std::string script_hash_example(
        "'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c  0x20 "
        "0xca139bc10c2f660da42666f72e89a225936fc60f19"
        "3c161124a672050c434671 0x88");
    BOOST_CHECK_EQUAL(bsv::execute(script_hash_example, true, 0),
                      SCRIPT_ERR_OK);

    // std::string script_hash(
    //        "'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 "
    //        "0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671"
    //        "OP_EQUALVERIFY");
    // BOOST_CHECK_EQUAL(bsv::execute(script_hash, true, 0), SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(test_case_2)
{
    // Please refer to the code in TestTxKeys.cpp to see where the generated
    // data came from
    // 71, 71 data bytes, 65, 65 data bytes, OP_DUP, OP_HASH160, 20, 20 data bytes
    // OP_EQUALVERIFY, OP_CHECKSIGVERIFY
    const std::string script{
        "0x47 "
        "0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f38"
        "86140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc36063"
        "7b01 0x41 "
        "0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b"
        "5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP "
        "HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY "
        "CHECKSIGVERIFY"};

    const std::string tx{
        "0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd551"
        "45e20000000000ffffffff010a000000000000000000000000"};

    constexpr auto amt{10};
    const auto status = bsv::execute(script, true, 0, tx, 0, amt);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_case_4)
{
    vector<uint8_t> script;
    ba::unhex("006B5455935987", back_inserter(script));

    std::vector<uint8_t> tx;
    ba::unhex("0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c9"
              "6cd55145e20000000000ffffffff010a000000000000000000000000",
              back_inserter(tx));

    constexpr auto amt{10};
    const auto status = bsv::execute(script, true, 0, tx, 0, amt);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_verify)
{
    vector<uint8_t> unlocking_script;
    ba::unhex("47" // opcode data length
                   // DER
              "30" // start
              "44" // length
              "02" // type
              "20" // length
              // R
              "543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c"
              "02" // type
              "20" // length
              // s
              "6204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f607"
              "01" // sighash
              "41" // opcode data length
              // pubkey
              "040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7"
              "447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a"
              "69",
              back_inserter(unlocking_script));

    vector<uint8_t> locking_script;
    ba::unhex("76" // OP_DUP
              "a9" // OP_HASH160
              "14" // data length 20
              "ff197b14e502ab41f3bc8ccb48c4abac9eab35bc"
              "88"  // OP_EQUALVERIFY
              "ac", // OP_CHECKSIG
              back_inserter(locking_script));

    vector<uint8_t> tx;
    ba::unhex("01000000" // version

              // Inputs
              "01" // n inputs (varint)
              // input 1
              // COutPoint
              // txid
              "d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e2"
              "00000000" // index
              // end COutPoint
              "00"       // CSript size (varint)
              "ffffffff" // nSequence
              // end input 1
              // end inputs

              // outputs
              "01"               // n outputs tx.vout
              "0a00000000000000" // amount
              "00"               // CScript size (varint)
              // end outputs

              "00000000", // locktime
              back_inserter(tx));

    constexpr int64_t amt{10}; // change this it doesn't fail!
    // const int flags{SCRIPT_ENABLE_SIGHASH_FORKID};
    const int flags{};
    const auto status =
        bsv::verify(unlocking_script, locking_script, true, flags, tx, 0, amt);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_verify_empty_script)
{
    vector<uint8_t> unlocking_script;
    vector<uint8_t> locking_script;

    vector<uint8_t> tx;
    ba::unhex("01000000" // version

              // Inputs
              "01" // n inputs (varint)
              // input 1
              // COutPoint
              // txid
              "d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e2"
              "00000000" // index
              // end COutPoint
              "00"       // CSript size (varint)
              "ffffffff" // nSequence
              // end input 1
              // end inputs

              // outputs
              "01"               // n outputs tx.vout
              "0a00000000000000" // amount
              "00"               // CScript size (varint)
              // end outputs

              "00000000", // locktime
              back_inserter(tx));

    constexpr int64_t amt{10};
    const int flags{};
    const auto status =
        bsv::verify(unlocking_script, locking_script, true, flags, tx, 0, amt);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_EVAL_FALSE, status);
}


BOOST_AUTO_TEST_CASE(test_verify_locking_script_empty)
{
    vector<uint8_t> unlocking_script;
    ba::unhex("47" // opcode data length
              // DER
              "30" // start
              "44" // length
              "02" // type
              "20" // length
              // R
              "543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c"
              "02" // type
              "20" // length
              // s
              "6204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f607"
              "01" // sighash
              "41" // opcode data length
              // pubkey
              "040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7"
              "447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a"
              "69",
              back_inserter(unlocking_script));

    vector<uint8_t> locking_script;

    vector<uint8_t> tx;
    ba::unhex("01000000" // version

              // Inputs
              "01" // n inputs (varint)
              // input 1
              // COutPoint
              // txid
              "d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e2"
              "00000000" // index
              // end COutPoint
              "00"       // CSript size (varint)
              "ffffffff" // nSequence
              // end input 1
              // end inputs

              // outputs
              "01"               // n outputs tx.vout
              "0a00000000000000" // amount
              "00"               // CScript size (varint)
              // end outputs

              "00000000", // locktime
              back_inserter(tx));

    constexpr int64_t amt{10};
    const int flags{};
    const auto status =
            bsv::verify(unlocking_script, locking_script, true, flags, tx, 0, amt);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_verify_extend)
{
    //This is on mainnet TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
    const std::string TxHexExtended = "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";
    const int32_t blockHeight = 108522;

    // Set mainnet
    const std::string err = bsv::SetGlobalScriptConfig(
        "main",
        int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0)
    );
    BOOST_CHECK_EQUAL(err.size(), 0);

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    const auto status = bsv::verify_extend(etx, blockHeight - 1, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_verify_extend_empty_utxos)
{
    // A extended transaction with zero input is expected to failed
    // This is on mainnet TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
    const std::string TxHexExtended = "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";
    const int32_t blockHeight = 108522;

    // Set mainnet
    const std::string err = bsv::SetGlobalScriptConfig(
        "main",
        int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0)
    );
    BOOST_CHECK_EQUAL(err.size(), 0);

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

    const auto status = bsv::verify_extend(emptyUtxoEtx, blockHeight - 1, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_UNKNOWN_ERROR, status);
}

BOOST_AUTO_TEST_CASE(test_verify_extend_full)
{
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the implementation of bsv::verify_extend_full
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::array<int32_t, 2> utxoArray = { 631924, 631924 };
    const int32_t blockHeight = 632099;

    // Set mainnet
    const std::string err = bsv::SetGlobalScriptConfig(
        "main",
        int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0)
    );
    BOOST_CHECK_EQUAL(err.size(), 0);

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());
    std::span<const int32_t> utxo(utxoArray);
    const auto status = bsv::verify_extend_full(etx, utxo, blockHeight - 1, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, status);
}

BOOST_AUTO_TEST_CASE(test_verify_extend_full_utxos)
{
    // A extended transaction with zero input is expected to failed
    // This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
    // This tx was whitelisted prior to the implementation of bsv::verify_extend_full
    const std::string TxHexExtended = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
    std::span<const int32_t> utxo;
    const int32_t blockHeight = 632099;

    // Set mainnet
    const std::string err = bsv::SetGlobalScriptConfig(
        "main",
        int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0), int64_t(0)
    );
    BOOST_CHECK_EQUAL(err.size(), 0);

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

    const auto status = bsv::verify_extend_full(emptyUtxoEtx, utxo, blockHeight - 1, true);
    BOOST_CHECK_EQUAL(SCRIPT_ERR_UNKNOWN_ERROR, status);
}

BOOST_AUTO_TEST_SUITE_END()
