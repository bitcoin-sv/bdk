/// test_sigopcount.cpp
/// C++ equivalents of teranode's:
///   services/validator/TxValidator_sigops_test.go  (TestCheckConsensusSigops_PreGenesis)
///   services/validator/TxValidator_test.go          (TestMaxTxSigopsCountsPolicy)

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_sigopcount
#else
#define BOOST_TEST_MODULE test_sigopcount_d
#endif

#include <boost/test/unit_test.hpp>

#include "streams.h"
#include "version.h"
#include "script/script.h"
#include "utilstrencodings.h"
#include "amount.h"
#include "primitives/transaction.h"

#include "txvalidator.hpp"
#include "extendedTx.hpp"
#include "txerror.h"
#include "doserror.hpp"

using namespace bsv;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> SerializeETX(const bsv::CMutableTransactionExtended& etx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << etx;
    return {ss.begin(), ss.end()};
}

// Build a minimal 1-in / 1-out extended tx.
// The UTXO script is empty (not P2SH), so P2SH sigops are never triggered.
// inputScript  : placed in vin[0].scriptSig
// outputScript : placed in vout[0].scriptPubKey
static std::vector<uint8_t> MakeETX(const CScript& inputScript,
                                     const CScript& outputScript)
{
    bsv::CMutableTransactionExtended etx;

    CTxIn txin;
    txin.scriptSig = inputScript;
    etx.mtx.vin.push_back(txin);
    etx.vutxo.emplace_back(Amount(1000), CScript{}); // empty UTXO — not P2SH

    etx.mtx.vout.emplace_back(Amount(500), outputScript);
    return SerializeETX(etx);
}

// Convenience: output-only sigops (scriptSig is empty)
static std::vector<uint8_t> MakeETXWithOutputSigOps(int n)
{
    CScript out;
    for (int i = 0; i < n; ++i) out << OP_CHECKSIG;
    return MakeETX(CScript{}, out);
}

// Convenience: split across scriptSig and output
static std::vector<uint8_t> MakeETXWithSplitSigOps(int nIn, int nOut)
{
    CScript in, out;
    for (int i = 0; i < nIn;  ++i) in  << OP_CHECKSIG;
    for (int i = 0; i < nOut; ++i) out << OP_CHECKSIG;
    return MakeETX(in, out);
}

// One utxoHeight = 0 for the single-input constructed transactions above.
static const std::array<int32_t, 1> kUtxoH0 = {0};
static const std::span<const int32_t> kOneUtxoH0(kUtxoH0);

// Helper to check DoS error code
static bool IsDosError(TxError r, DoSError_t code)
{
    return r.domain == TX_ERR_DOMAIN_DOS &&
           r.code   == static_cast<int32_t>(code);
}

// ---------------------------------------------------------------------------
// SUITE 1 — CheckConsensusSigops
// Mirrors TestCheckConsensusSigops_PreGenesis (TxValidator_sigops_test.go)
// Genesis at height 1000; tests use height 999 (pre) or 1000 (post).
// These tests call CheckConsensusSigops directly, matching the Go test which
// calls checkConsensusSigops directly (not via ValidateTransaction).
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(check_consensus_sigops)

// Shared fixture: validator with genesis=1000
struct Fixture {
    bsv::CTxValidator v{"main"};
    Fixture() {
        std::string err;
        v.SetGenesisActivationHeight(1000, &err);
    }
};

BOOST_FIXTURE_TEST_CASE(pre_genesis_allows_tx_under_sigops_limit, Fixture)
{
    // 10 OP_CHECKSIG in output — well under 20,000 limit
    auto etxBin = MakeETXWithOutputSigOps(10);
    std::span<const uint8_t> etx(etxBin);
    auto r = v.CheckConsensusSigops(etx, kOneUtxoH0, 999);
    BOOST_CHECK(TxErrorIsOk(r));
}

BOOST_FIXTURE_TEST_CASE(pre_genesis_rejects_tx_exceeding_sigops_limit, Fixture)
{
    // 20,001 OP_CHECKSIG — exceeds 20,000 consensus limit
    auto etxBin = MakeETXWithOutputSigOps(20001);
    std::span<const uint8_t> etx(etxBin);
    auto r = v.CheckConsensusSigops(etx, kOneUtxoH0, 999);
    BOOST_CHECK(IsDosError(r, DoSError_t::SigopsConsensus));
}

BOOST_FIXTURE_TEST_CASE(pre_genesis_allows_exactly_20000_sigops, Fixture)
{
    // Exactly 20,000 OP_CHECKSIG — at the limit, should pass
    auto etxBin = MakeETXWithOutputSigOps(20000);
    std::span<const uint8_t> etx(etxBin);
    auto r = v.CheckConsensusSigops(etx, kOneUtxoH0, 999);
    BOOST_CHECK(TxErrorIsOk(r));
}

BOOST_FIXTURE_TEST_CASE(post_genesis_allows_unlimited_sigops, Fixture)
{
    // 100,000 OP_CHECKSIG — way over pre-Genesis limit, but post-Genesis is unlimited
    auto etxBin = MakeETXWithOutputSigOps(100000);
    std::span<const uint8_t> etx(etxBin);
    auto r = v.CheckConsensusSigops(etx, kOneUtxoH0, 1000); // blockHeight=1000 → post-Genesis
    BOOST_CHECK(TxErrorIsOk(r));
}

BOOST_FIXTURE_TEST_CASE(counts_sigops_in_both_inputs_and_outputs, Fixture)
{
    // 5,000 in scriptSig + 15,001 in output = 20,001 total → should fail
    auto etxBin = MakeETXWithSplitSigOps(5000, 15001);
    std::span<const uint8_t> etx(etxBin);
    auto r = v.CheckConsensusSigops(etx, kOneUtxoH0, 999);
    BOOST_CHECK(IsDosError(r, DoSError_t::SigopsConsensus));
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// SUITE 2 — CheckSigOpsPolicy
// Mirrors TestMaxTxSigopsCountsPolicy (TxValidator_test.go).
//
// teranode's ValidateTransaction calls sigOpsCheck but NOT ValidateTransactionScripts,
// so these tests use CheckSigOpsPolicy directly (not CheckTransaction) to match.
//
// Transaction structure (1 P2SH input, 3 P2PKH outputs):
//   - P2SH redeem script: 2-of-3 multisig → 3 sigops when UTXO is pre-Genesis
//   - 3 P2PKH outputs × 1 sigop each = 3 sigops always
//
// Sigop totals:
//   - Post-Genesis UTXO (height=1000): 3 sigops (P2SH redeem ignored)
//   - Pre-Genesis  UTXO (height=999):  6 sigops (3 outputs + 3 P2SH redeem)
//
// The tx hex is BIP-239 extended format (includes the embedded UTXO).
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(check_sigops_policy)

static const std::string kTestTxHex =
    "010000000000000000ef01074024ca185c3dea94748dc93bb369e82816bed423ad9ebcd41e112dc984f307"
    "00000000fd5d01004730440220320f4c56a764c80d7cddb930026b8af9cf00e88aa934710c969aa23883ec"
    "45bb0220701d81304f88258e116d3c9e988d6879839c805e7bdd96a17d56bdae115ff56541483045022100"
    "a3147b9488006b6952b39d9c432dacb8984f4fd59187a41f041f089355e1217502206f643471e74005f039"
    "323533b8a80b82c12bb3706a4b63f4bfba90df4bc39e97414cc95241040b4c866585dd868a9d62348a9cd0"
    "08d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a"
    "19f694054e5a694104183905ae25e815634ce7f5d9bedbaa2c39032ab98c75b5e88fe43f8dd8246f3c5473"
    "ccd4ab475e6a9e6620b52f5ce2fd15a2de32cbe905154b3a05844af707854104f028892bad7ed57d2fb57b"
    "f33081d5cfcf6f9ed3d3d7f159c2e2fff579dc341a07cf33da18bd734c600b96a72bbc4749d5141c90ec8a"
    "c328ae52ddfe2e505bdb53aeffffffff60e316000000000017a914c51a96cac717c6b1bc2d6c65a6b5cc88"
    "9d6a5b43870320a10700000000001976a914ff197b14e502ab41f3bc8ccb48c4abac9eab35bc88ac20a107"
    "00000000001976a9149a823b698f778ece90b094dc3f12a81f5e3c334588ac20a10700000000001976a914"
    "211b74ca4686f81efda5641767fc84ef16dafe0b88ac00000000";

struct PolicyFixture {
    std::vector<uint8_t> etxBin;
    std::span<const uint8_t> etx;

    bsv::CTxValidator MakeValidator(uint64_t sigOpsLimit) const {
        bsv::CTxValidator v("main");
        std::string err;
        v.SetGenesisActivationHeight(1000, &err);
        v.SetMaxSigOpsPolicy(sigOpsLimit);
        return v;
    }

    PolicyFixture() {
        etxBin = ParseHex(kTestTxHex);
        etx    = std::span<const uint8_t>(etxBin);
    }
};

// All tests use blockHeight=998 so blockHeight+1=999 < genesis=1000 → pre-Genesis era → check runs.
// The UTXO height controls whether P2SH redeem scripts are counted (pre-Genesis UTXO) or not.

// -- Scenario 1: Post-Genesis UTXO (height=1000) → P2SH redeem ignored → 3 sigops from outputs only --

BOOST_FIXTURE_TEST_CASE(policy_post_genesis_utxo_limit2_fails, PolicyFixture)
{
    // 3 sigops > policy limit 2 → fail
    std::array<int32_t, 1> uh = {1000};
    auto v = MakeValidator(2);
    auto r = v.CheckSigOpsPolicy(etx, std::span<const int32_t>(uh), 998);
    BOOST_CHECK(IsDosError(r, DoSError_t::SigopsPolicy));
}

BOOST_FIXTURE_TEST_CASE(policy_post_genesis_utxo_limit3_passes, PolicyFixture)
{
    // 3 sigops == policy limit 3 → pass
    std::array<int32_t, 1> uh = {1000};
    auto v = MakeValidator(3);
    auto r = v.CheckSigOpsPolicy(etx, std::span<const int32_t>(uh), 998);
    BOOST_CHECK(TxErrorIsOk(r));
}

// -- Scenario 2: Pre-Genesis UTXO (height=999) → P2SH redeem counted → 3 outputs + 3 P2SH = 6 sigops --

BOOST_FIXTURE_TEST_CASE(p2sh_pre_genesis_utxo_limit5_fails, PolicyFixture)
{
    // 6 sigops > policy limit 5 → fail
    std::array<int32_t, 1> uh = {999};
    auto v = MakeValidator(5);
    auto r = v.CheckSigOpsPolicy(etx, std::span<const int32_t>(uh), 998);
    BOOST_CHECK(IsDosError(r, DoSError_t::SigopsPolicy));
}

BOOST_FIXTURE_TEST_CASE(p2sh_pre_genesis_utxo_limit6_passes, PolicyFixture)
{
    // 6 sigops == policy limit 6 → pass
    std::array<int32_t, 1> uh = {999};
    auto v = MakeValidator(6);
    auto r = v.CheckSigOpsPolicy(etx, std::span<const int32_t>(uh), 998);
    BOOST_CHECK(TxErrorIsOk(r));
}

BOOST_FIXTURE_TEST_CASE(p2sh_post_genesis_utxo_ignored_passes, PolicyFixture)
{
    // Post-Genesis UTXO → P2SH redeem ignored → 3 sigops, limit=6 → pass
    std::array<int32_t, 1> uh = {1000};
    auto v = MakeValidator(6);
    auto r = v.CheckSigOpsPolicy(etx, std::span<const int32_t>(uh), 998);
    BOOST_CHECK(TxErrorIsOk(r));
}

BOOST_AUTO_TEST_SUITE_END()
