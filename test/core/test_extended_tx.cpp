#include <memory>
#include <string>
#include <vector>

/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_extendedtx
#else
#define BOOST_TEST_MODULE test_extendedtxd
#endif

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>

#include "extendedTx.hpp"
#include "assembler.h"

BOOST_AUTO_TEST_SUITE(test_extended_tx_suite)

BOOST_AUTO_TEST_CASE(serialization)
{
    const std::string TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
    const std::string TxHex = "010000000231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffffdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";
    const std::string TxHexExtended = "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";

    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());

    const char* begin{ reinterpret_cast<const char*>(etx.data()) };
    const char* end{ reinterpret_cast<const char*>(etx.data() + etx.size()) };
    CDataStream is(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    is>> eTX;

    ////Recover the extended hex
    CDataStream os1(SER_NETWORK, PROTOCOL_VERSION);
    os1 << eTX;
    const std::vector<uint8_t> extendedBin(os1.begin(), os1.end());
    const std::string recovTxHexExtended = bsv::Bin2Hex(extendedBin);

    BOOST_CHECK_EQUAL(TxHexExtended, recovTxHexExtended);

    const std::string recovTxID = eTX.mtx.GetId().ToString();
    BOOST_CHECK_EQUAL(TxID, recovTxID);

    ////Recover the std hex
    CDataStream os2(SER_NETWORK, PROTOCOL_VERSION);
    os2 << eTX.mtx;
    const std::vector<uint8_t> stdBin(os2.begin(), os2.end());
    const std::string recovTxHex = bsv::Bin2Hex(stdBin);
    BOOST_CHECK_EQUAL(TxHex, recovTxHex);
}

// CompactSizeWidth is the number of bytes bitcoin uses to encode a byte count.
static size_t CompactSizeWidth(size_t n)
{
    if (n < 253) {
        return 1;
    }
    if (n <= 0xffff) {
        return 3;
    }
    if (n <= 0xffffffff) {
        return 5;
    }
    return 9;
}

// The extended representation is the bare transaction, plus six marker bytes, plus per
// input eight amount bytes, the CompactSize length of the previous locking script, and
// the script itself. That identity is what turns a small transaction into a
// multi-gigabyte buffer, so it is asserted here for bdk's own serialiser, with the
// script sizes straddling every CompactSize threshold.
BOOST_AUTO_TEST_CASE(extended_length_identity_across_compactsize_thresholds)
{
    const std::vector<size_t> scriptSizes{ 1, 252, 253, 0xffff, 0x10000 };

    bsv::CMutableTransactionExtended eTX;
    eTX.mtx.nVersion = 2;
    eTX.mtx.nLockTime = 0;
    eTX.mtx.vin.resize(scriptSizes.size());
    eTX.vutxo.resize(scriptSizes.size());

    for (size_t i = 0; i < scriptSizes.size(); ++i) {
        eTX.mtx.vin[i].prevout = COutPoint(uint256S("01"), static_cast<uint32_t>(i));
        eTX.mtx.vin[i].nSequence = 0xffffffff;
        eTX.vutxo[i].nValue = Amount(1000);

        const std::vector<uint8_t> raw(scriptSizes[i], uint8_t{ 0x01 });
        eTX.vutxo[i].scriptPubKey = CScript(raw.begin(), raw.end());
        BOOST_REQUIRE_EQUAL(scriptSizes[i], static_cast<size_t>(eTX.vutxo[i].scriptPubKey.size()));
    }

    eTX.mtx.vout.resize(1);
    eTX.mtx.vout[0].nValue = Amount(1);
    eTX.mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CDataStream bare(SER_NETWORK, PROTOCOL_VERSION);
    bare << eTX.mtx;

    CDataStream extended(SER_NETWORK, PROTOCOL_VERSION);
    extended << eTX;

    size_t expected = bare.size() + 6;
    for (const size_t scriptSize : scriptSizes) {
        expected += 8 + CompactSizeWidth(scriptSize) + scriptSize;
    }

    BOOST_CHECK_EQUAL(expected, extended.size());
}

BOOST_AUTO_TEST_SUITE_END()
