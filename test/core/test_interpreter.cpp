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

namespace ba = boost::algorithm;

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_interpreter)

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

BOOST_AUTO_TEST_SUITE_END()
