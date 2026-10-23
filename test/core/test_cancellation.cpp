#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

/// Define test module name with debug postfix
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_cancellation
#else
#define BOOST_TEST_MODULE test_cancellationd
#endif

#include <boost/test/unit_test.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#include "chainparamsbase.h"
#include "primitives/transaction.h"
#include "hash.h"
#include "key.h"
#include "pubkey.h"
#include "script/interpreter.h"
#include "script/sighashtype.h"
#include "script/script.h"
#include "streams.h"
#include "taskcancellation.h"
#include "utilstrencodings.h"
#include "version.h"

#include "extendedTx.hpp"

#include "cancellation.hpp"
#include "txvalidator.hpp"
#include "validatearg.hpp"
#include "txerror.h"

BOOST_AUTO_TEST_SUITE(test_cancellation)

namespace {

// Mainnet tx 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105, extended,
// valid in consensus mode at blockHeight 632099 (same fixture as test_txvalidator).
constexpr const char* kValidTxHex = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
constexpr int32_t kValidTxBlockHeight = 632099;
constexpr std::array<int32_t, 2> kValidTxUtxoHeights{ 631924, 631924 };

task::CCancellationToken CancelledToken()
{
    auto source = task::CCancellationSource::Make();
    source->Cancel();
    return source->GetToken();
}

// Sets a cancel flag the way a binding does: an atomic store from another thread.
void StoreFlagRelaxed(int32_t* flag, int32_t value)
{
#if defined(_MSC_VER) && !defined(__clang__)
    __iso_volatile_store32(reinterpret_cast<volatile __int32*>(flag), value);
#else
    __atomic_store_n(flag, value, __ATOMIC_RELAXED);
#endif
}

bool IsCancelled(const TxError& r)
{
    return r.domain == static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED);
}

// Regtest heights at which the spent output and the block are post-Chronicle.
constexpr int32_t kLongBlockHeight = 20000;
constexpr std::array<int32_t, 1> kLongUtxoHeights{ 19000 };

// A valid one-input transaction whose script runs for seconds: the unlocking script
// pushes a 1 MB element and the locking script hashes it `rounds` times
// (OP_DUP OP_SHA256 OP_DROP), so the interpreter polls its token every few ms.
std::vector<uint8_t> LongRunningTx(size_t rounds)
{
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.vin.resize(1);
    eTx.mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    eTx.mtx.vin[0].scriptSig = CScript() << std::vector<uint8_t>(1 << 20, 0x01);
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(1000);
    eTx.mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    eTx.vutxo.resize(1);
    eTx.vutxo[0].nValue = Amount(100000);
    for (size_t i = 0; i < rounds; ++i)
        eTx.vutxo[0].scriptPubKey << OP_DUP << OP_SHA256 << OP_DROP;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

// A regtest validator whose policy admits LongRunningTx.
bsv::CTxValidator LongRunningValidator()
{
    bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    std::string err;
    BOOST_REQUIRE(validator.SetMaxScriptSizePolicy(4 << 20, &err));
    BOOST_REQUIRE(validator.SetMaxOpsPerScriptPolicy(1000000, &err));
    return validator;
}

// Runs call() while another thread cancels source after delay; returns the result
// and how long call() took after the cancel.
template <typename Call>
std::pair<TxError, std::chrono::milliseconds> RunAndCancelAfter(
    const std::shared_ptr<task::CCancellationSource>& source,
    std::chrono::milliseconds delay,
    Call call)
{
    using clock = std::chrono::steady_clock;
    std::atomic<int64_t> cancelledAtNs{ 0 };
    std::thread canceller([&] {
        std::this_thread::sleep_for(delay);
        cancelledAtNs = clock::now().time_since_epoch().count();
        source->Cancel();
    });
    const TxError result = call();
    const auto returnedAt = clock::now();
    canceller.join();
    const auto cancelledAt = clock::time_point(clock::duration(cancelledAtNs.load()));
    return { result, std::chrono::duration_cast<std::chrono::milliseconds>(returnedAt - cancelledAt) };
}

// A source that reports cancelled from its (limit+1)-th poll on: the cancel lands at
// an exact, repeatable point of the call. polls() counts every poll.
class CountdownSource final : public task::CCancellationSource {
public:
    static std::shared_ptr<CountdownSource> Make(uint64_t limit)
    {
        return std::shared_ptr<CountdownSource>(new CountdownSource(limit));
    }
    bool IsCanceled() override { return ++polls_ > limit_; }
    uint64_t polls() const { return polls_; }

private:
    explicit CountdownSource(uint64_t limit) : limit_{ limit } {}
    const uint64_t limit_;
    std::atomic<uint64_t> polls_{ 0 };
};

// Pre-Genesis P2SH spend (redeem script OP_TRUE) paying a P2PKH output: standard, so
// policy mode with standardness required runs IsInputStandard on its input.
std::vector<uint8_t> PreGenesisP2SHTx()
{
    const CScript redeem = CScript() << OP_TRUE;
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 1;
    eTx.mtx.vin.resize(1);
    eTx.mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    eTx.mtx.vin[0].scriptSig = CScript() << std::vector<uint8_t>(redeem.begin(), redeem.end());
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(90000);
    eTx.mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << std::vector<uint8_t>(20, 0x11)
                                             << OP_EQUALVERIFY << OP_CHECKSIG;
    eTx.vutxo.resize(1);
    eTx.vutxo[0].nValue = Amount(100000);
    const uint160 redeemHash = Hash160(redeem.begin(), redeem.end());
    eTx.vutxo[0].scriptPubKey = CScript() << OP_HASH160 << std::vector<uint8_t>(redeemHash.begin(), redeemHash.end())
                                          << OP_EQUAL;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

// A source that counts its polls and records when each happened. With cancelAfter
// set, it cancels itself right after answering that poll, so the cancel lands in
// whatever the call does between that poll and the next one.
class RecordingSource final : public task::CCancellationSource {
public:
    using clock = std::chrono::steady_clock;
    static std::shared_ptr<RecordingSource> Make(uint64_t cancelAfter = UINT64_MAX)
    {
        return std::shared_ptr<RecordingSource>(new RecordingSource(cancelAfter));
    }
    bool IsCanceled() override
    {
        const uint64_t n = ++polls_;
        if (n < times_.size())
            times_[n] = clock::now();
        const bool cancelled = CCancellationSource::IsCanceled();
        if (n == cancelAfter_)
            Cancel();
        return cancelled;
    }
    uint64_t polls() const { return polls_; }
    clock::time_point pollTime(uint64_t n) const { return times_.at(n); }

private:
    explicit RecordingSource(uint64_t cancelAfter) : cancelAfter_{ cancelAfter }, times_(64) {}
    const uint64_t cancelAfter_;
    std::atomic<uint64_t> polls_{ 0 };
    std::vector<clock::time_point> times_;
};

// Two inputs, each spending <scriptCodeBytes push> OP_DROP <pubkey> OP_CHECKSIG with a
// FORKID signature: every OP_CHECKSIG hashes the whole locking script, a single step
// with no poll inside. Mainnet, post-Genesis.
constexpr int32_t kSighashBlockHeight = 800000;
constexpr std::array<int32_t, 2> kSighashUtxoHeights{ 700000, 700000 };

std::vector<uint8_t> LargeSighashTx(size_t scriptCodeBytes)
{
    constexpr std::array<uint8_t, 32> rawKey = {
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x7a };
    CKey key;
    key.Set(rawKey.begin(), rawKey.end(), /*fCompressedIn=*/true);
    BOOST_REQUIRE(key.IsValid());
    const CPubKey pubkey = key.GetPubKey();

    const CScript lockingScript = CScript() << std::vector<uint8_t>(scriptCodeBytes, 0x01) << OP_DROP
                                            << std::vector<uint8_t>(pubkey.begin(), pubkey.end()) << OP_CHECKSIG;
    constexpr int64_t amount = 100000;

    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.vin.resize(2);
    eTx.vutxo.resize(2);
    for (uint32_t i = 0; i < 2; ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("01"), i);
        eTx.vutxo[i].nValue = Amount(amount);
        eTx.vutxo[i].scriptPubKey = lockingScript;
    }
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(1000);
    eTx.mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;

    const SigHashType sigHashType = SigHashType().withForkId();
    for (uint32_t i = 0; i < 2; ++i) {
        const uint256 sighash = SignatureHash(lockingScript, CTransaction(eTx.mtx), i, sigHashType, Amount(amount));
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(key.Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
        eTx.mtx.vin[i].scriptSig = CScript() << sig;
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

// One input, one output per entry of outputValues, mainnet post-Genesis heights
// (kSimpleBlockHeight / kSimpleUtxoHeights). Scripts are given verbatim.
constexpr int32_t kSimpleBlockHeight = 800000;
constexpr std::array<int32_t, 1> kSimpleUtxoHeights{ 700000 };

std::vector<uint8_t> SimpleTx(const CScript& scriptSig, const CScript& lockingScript,
                              int64_t inputValue, const std::vector<int64_t>& outputValues)
{
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.vin.resize(1);
    eTx.mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    eTx.mtx.vin[0].scriptSig = scriptSig;
    eTx.vutxo.resize(1);
    eTx.vutxo[0].nValue = Amount(inputValue);
    eTx.vutxo[0].scriptPubKey = lockingScript;
    for (const int64_t value : outputValues) {
        CTxOut out;
        out.nValue = Amount(value);
        out.scriptPubKey = CScript() << OP_TRUE;
        eTx.mtx.vout.push_back(out);
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

// A verdict from each phase of ValidateTransaction that has no poll inside it.
// No outputs: rejected by the transaction-level checks right after deserialising.
std::vector<uint8_t> NoOutputsTx() { return SimpleTx(CScript() << OP_TRUE, CScript(), 100000, {}); }
// Outputs worth more than the input: rejected by the input-value check, after the
// output checks.
std::vector<uint8_t> InputsBelowOutputsTx() { return SimpleTx(CScript() << OP_TRUE, CScript(), 100, { 1000 }); }
// Empty unlocking and locking scripts: the interpreter runs no opcode, so it never
// polls, and script verification fails on the empty stack.
std::vector<uint8_t> EmptyScriptsTx() { return SimpleTx(CScript(), CScript(), 100000, { 1000 }); }

// Pre-Genesis standardness fixtures (regtest, block 5000, inputs from 4000), for a
// validator with standardness required. P2PKH inputs need no script evaluation to be
// standard, so the interpreter never polls while checking them.
constexpr std::array<int32_t, 2> kPreGenesisUtxoHeights2{ 4000, 4000 };
constexpr std::array<int32_t, 1> kPreGenesisP2SHUtxoHeights{ 4000 };
CScript P2PKHScript()
{
    return CScript() << OP_DUP << OP_HASH160 << std::vector<uint8_t>(20, 0x11) << OP_EQUALVERIFY << OP_CHECKSIG;
}

std::vector<uint8_t> PreGenesisTwoP2PKHInputsTx(const CScript& outputScript)
{
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 1;
    eTx.mtx.vin.resize(2);
    eTx.vutxo.resize(2);
    for (uint32_t i = 0; i < 2; ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("01"), i);
        eTx.vutxo[i].nValue = Amount(100000);
        eTx.vutxo[i].scriptPubKey = P2PKHScript();
    }
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(90000);
    eTx.mtx.vout[0].scriptPubKey = outputScript;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

// An ordinary (not extended) one-input transaction with an empty unlocking script.
std::vector<uint8_t> PlainEmptyScriptSigTx()
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = Amount(1000);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << mtx;
    return { ss.begin(), ss.end() };
}

} // namespace

// A token that has already fired stops the call before it reads the transaction: the
// result is the cancelled domain for well-formed and malformed input alike, in both
// modes, and never a verdict.
BOOST_AUTO_TEST_CASE(cancelled_before_start_returns_cancelled)
{
    const std::vector<uint8_t> valid = ParseHex(kValidTxHex);
    const std::vector<uint8_t> malformed{ 0x01, 0x02, 0x03 };
    const auto token = CancelledToken();
    const bsv::CTxValidator validator("main");

    for (const bool consensus : { true, false }) {
        for (const auto* tx : { &valid, &malformed }) {
            const std::span<const uint8_t> etx(tx->data(), tx->size());
            BOOST_CHECK(IsCancelled(validator.ValidateTransaction(etx, kValidTxUtxoHeights, kValidTxBlockHeight, consensus, token)));
        }
    }
}

// Every other cancellable entry point honours a token that fired before the call,
// on malformed input too.
BOOST_AUTO_TEST_CASE(every_entry_point_cancelled_before_start_returns_cancelled)
{
    const std::vector<uint8_t> valid = ParseHex(kValidTxHex);
    const std::vector<uint8_t> malformed{ 0x01, 0x02, 0x03 };
    const std::vector<uint32_t> flags{ 869935, 869935 };
    const std::vector<uint8_t> p2pkh = ParseHex("76a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac");
    const auto token = CancelledToken();
    const bsv::CTxValidator validator("main");

    for (const bool consensus : { true, false }) {
        for (const auto* tx : { &valid, &malformed }) {
            const std::span<const uint8_t> etx(tx->data(), tx->size());
            BOOST_TEST_CONTEXT("consensus=" << consensus << " size=" << tx->size()) {
                BOOST_CHECK(IsCancelled(validator.VerifyScript(etx, kValidTxUtxoHeights, kValidTxBlockHeight, consensus, {}, token)));
                BOOST_CHECK(IsCancelled(validator.VerifyScript(etx, kValidTxUtxoHeights, kValidTxBlockHeight, consensus, flags, token)));
                BOOST_CHECK(IsCancelled(validator.VerifySpend(etx, 0, p2pkh, 1000, kValidTxUtxoHeights[0], kValidTxBlockHeight, consensus, std::nullopt, token)));
                BOOST_CHECK(IsCancelled(validator.CheckStandardness(etx, kValidTxUtxoHeights, kValidTxBlockHeight, token)));

                bsv::ValidateBatch batch;
                batch.add(bsv::ValidateArg(etx, kValidTxUtxoHeights, kValidTxBlockHeight, consensus));
                batch.add(bsv::ValidateArg(etx, kValidTxUtxoHeights, kValidTxBlockHeight, consensus));
                const auto results = validator.ValidateBatch(batch, token);
                BOOST_REQUIRE_EQUAL(2u, results.size());
                BOOST_CHECK(IsCancelled(results[0]));
                BOOST_CHECK(IsCancelled(results[1]));
            }
        }
    }
}

// A batch cancelled part-way keeps the verdicts it reached and reports the rest as
// cancelled, in order.
BOOST_AUTO_TEST_CASE(batch_cancelled_part_way_keeps_finished_verdicts)
{
    const std::vector<uint8_t> valid = ParseHex(kValidTxHex);
    std::vector<uint8_t> invalid = valid;
    invalid[60] ^= 0x01;
    const std::span<const uint8_t> validTx(valid.data(), valid.size());
    const std::span<const uint8_t> invalidTx(invalid.data(), invalid.size());
    const bsv::CTxValidator validator("main");

    bsv::ValidateBatch batch;
    batch.add(bsv::ValidateArg(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true));
    batch.add(bsv::ValidateArg(invalidTx, kValidTxUtxoHeights, kValidTxBlockHeight, true));
    batch.add(bsv::ValidateArg(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true));
    batch.add(bsv::ValidateArg(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true));

    // Polls taken by the first two entries, so the token fires at the third.
    const auto counter = CountdownSource::Make(UINT64_MAX);
    validator.ValidateTransaction(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true, counter->GetToken());
    validator.ValidateTransaction(invalidTx, kValidTxUtxoHeights, kValidTxBlockHeight, true, counter->GetToken());

    const auto source = CountdownSource::Make(counter->polls());
    const auto results = validator.ValidateBatch(batch, source->GetToken());
    BOOST_REQUIRE_EQUAL(4u, results.size());
    BOOST_CHECK(bsv::TxErrorIsOk(results[0]));
    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT), results[1].domain);
    BOOST_CHECK(IsCancelled(results[2]));
    BOOST_CHECK(IsCancelled(results[3]));
}

// A token cancelled while the script runs stops the call at the interpreter's next
// poll. The script takes seconds; the call must return well inside that, without a
// verdict, in both modes.
BOOST_AUTO_TEST_CASE(cancelled_mid_script_returns_cancelled_promptly)
{
    const std::vector<uint8_t> tx = LongRunningTx(2000);
    const std::span<const uint8_t> etx(tx.data(), tx.size());
    const bsv::CTxValidator validator = LongRunningValidator();

    for (const bool consensus : { true, false }) {
        auto source = task::CCancellationSource::Make();
        const auto token = source->GetToken();
        const auto [result, latency] = RunAndCancelAfter(source, std::chrono::milliseconds(50), [&] {
            return validator.ValidateTransaction(etx, kLongUtxoHeights, kLongBlockHeight, consensus, token);
        });
        std::cerr << "[cancelled_mid_script] consensus=" << consensus << " domain=" << result.domain
                  << " latency_ms=" << latency.count() << '\n';
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), result.domain);
        BOOST_CHECK_LT(latency.count(), 1000);
    }
}

// Every poll is a place the call can stop. For each k, a token that fires at poll
// k+1 must give exactly Cancelled when the uncancelled run polls more than k times,
// and the uncancelled result otherwise: a cancel never turns into a verdict, an
// exception or a different verdict. The cases cover a verdict of OK, script
// failures, transaction-level rejections, and policy mode running IsInputStandard
// on a P2SH input.
BOOST_AUTO_TEST_CASE(cancel_at_every_poll_gives_cancelled_or_the_uncancelled_result)
{
    struct Case {
        const char* name;
        std::vector<uint8_t> tx;
        std::span<const int32_t> utxoHeights; // views a static array
        int32_t blockHeight;
        std::string chain;
        bool requireStandard;  // pre-Genesis, non-standard outputs are refused when true
    };
    std::vector<uint8_t> invalid = ParseHex(kValidTxHex);
    invalid[60] ^= 0x01;  // first input's signature
    const std::vector<Case> cases{
        { "valid", ParseHex(kValidTxHex), kValidTxUtxoHeights, kValidTxBlockHeight, CBaseChainParams::MAIN, false },
        { "bad-signature", invalid, kValidTxUtxoHeights, kValidTxBlockHeight, CBaseChainParams::MAIN, false },
        { "pre-genesis-p2sh", PreGenesisP2SHTx(), kPreGenesisP2SHUtxoHeights, 5000, CBaseChainParams::REGTEST, true },
        { "no-outputs", NoOutputsTx(), kSimpleUtxoHeights, kSimpleBlockHeight, CBaseChainParams::MAIN, false },
        { "inputs-below-outputs", InputsBelowOutputsTx(), kSimpleUtxoHeights, kSimpleBlockHeight, CBaseChainParams::MAIN, false },
        { "empty-scripts", EmptyScriptsTx(), kSimpleUtxoHeights, kSimpleBlockHeight, CBaseChainParams::MAIN, false },
    };

    for (const auto& c : cases) {
        bsv::CTxValidator validator(c.chain);
        if (c.requireStandard)
            validator.SetRequireStandard(true);
        const std::span<const uint8_t> etx(c.tx.data(), c.tx.size());
        for (const bool consensus : { true, false }) {
            const auto counter = CountdownSource::Make(UINT64_MAX);
            const TxError expected = validator.ValidateTransaction(etx, c.utxoHeights, c.blockHeight, consensus, counter->GetToken());
            const uint64_t polls = counter->polls();
            std::cerr << "[cancel_at_every_poll] " << c.name << " consensus=" << consensus << " polls=" << polls
                      << " result=" << expected.domain << "/" << expected.code << '\n';
            BOOST_TEST_CONTEXT(c.name << " consensus=" << consensus << " polls=" << polls) {
                BOOST_CHECK_NE(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION), expected.domain);
                for (uint64_t k = 0; k <= polls; ++k) {
                    const auto source = CountdownSource::Make(k);
                    const TxError r = validator.ValidateTransaction(etx, c.utxoHeights, c.blockHeight, consensus, source->GetToken());
                    BOOST_TEST_CONTEXT("k=" << k) {
                        if (k < polls) {
                            BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), r.domain);
                        } else {
                            BOOST_CHECK_EQUAL(expected.domain, r.domain);
                            BOOST_CHECK_EQUAL(expected.code, r.code);
                        }
                    }
                }
            }
        }
    }
}

// ValidateTransaction polls at entry, after deserialising, after the output checks
// and after building the shared signature-hash data, before the scripts. Each of the
// three polls after entry ends a phase that has no poll inside it. A cancel that lands in such a phase must stop the call at the
// poll that ends it, before the next phase can reach a verdict. Each case cancels
// right after the poll that starts a phase (the 1st, 2nd or 3rd poll of the call)
// and uses a transaction whose verdict comes from the phase after it, so dropping
// the poll in between turns the expected Cancelled into that verdict.
BOOST_AUTO_TEST_CASE(cancel_during_each_unpolled_phase_is_honoured_before_the_next_verdict)
{
    struct Case {
        const char* phase;          // where the cancel lands
        uint64_t cancelAfterPoll;   // the poll that starts that phase
        std::vector<uint8_t> tx;    // its uncancelled verdict comes from the next phase
        int32_t verdictDomain;
    };
    const std::vector<Case> cases{
        { "deserialising", 1, NoOutputsTx(), static_cast<int32_t>(TX_ERR_DOMAIN_DOS) },
        { "transaction and output checks", 2, InputsBelowOutputsTx(), static_cast<int32_t>(TX_ERR_DOMAIN_DOS) },
        { "input value and fee checks and building the shared signature-hash data", 3, EmptyScriptsTx(), static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT) },
    };
    const bsv::CTxValidator validator("main");

    for (const auto& c : cases) {
        const std::span<const uint8_t> etx(c.tx.data(), c.tx.size());
        BOOST_TEST_CONTEXT("cancel during " << c.phase) {
            // The uncancelled verdict, so a missing poll shows up as exactly that.
            const TxError verdict = validator.ValidateTransaction(etx, kSimpleUtxoHeights, kSimpleBlockHeight, true);
            BOOST_REQUIRE_EQUAL(c.verdictDomain, verdict.domain);

            const auto source = RecordingSource::Make(c.cancelAfterPoll);
            const TxError r = validator.ValidateTransaction(etx, kSimpleUtxoHeights, kSimpleBlockHeight, true, source->GetToken());
            BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), r.domain);
            BOOST_CHECK_EQUAL(c.cancelAfterPoll + 1, source->polls());
        }
    }
}

// The same property for VerifySpend, VerifyScript and CheckStandardness. A cancel
// that lands while they deserialise is seen right after, before any check that can
// return a result; in VerifySpend one that lands while it builds the shared
// signature-hash data is seen before the script; in CheckStandardness one that lands
// before an input's standardness check is seen before that check. Each case's
// uncancelled result needs no further poll, so a missing poll turns the expected
// Cancelled into that result.
BOOST_AUTO_TEST_CASE(verify_spend_and_check_standardness_poll_after_deserialising_and_per_input)
{
    const bsv::CTxValidator mainValidator("main");
    {
        // Empty unlocking and locking scripts: no opcode, so no interpreter poll.
        const std::vector<uint8_t> tx = PlainEmptyScriptSigTx();
        const std::vector<uint8_t> emptyLock;
        const auto verdict = mainValidator.VerifySpend(tx, 0, emptyLock, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, true);
        BOOST_REQUIRE_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_SCRIPT), verdict.domain);

        const auto source = RecordingSource::Make(1);  // cancel lands while deserialising
        const auto r = mainValidator.VerifySpend(tx, 0, emptyLock, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, true, std::nullopt, source->GetToken());
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), r.domain);
        BOOST_CHECK_EQUAL(2u, source->polls());

        // Cancel lands while the shared signature-hash data is built.
        const auto during = RecordingSource::Make(2);
        const auto d = mainValidator.VerifySpend(tx, 0, emptyLock, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, true, std::nullopt, during->GetToken());
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), d.domain);
        BOOST_CHECK_EQUAL(3u, during->polls());

        // Checks right after deserialising that return a result: an unconfirmed input
        // in consensus mode (a DoS result) and an input index out of range (an
        // exception). A cancel that lands while deserialising wins over both.
        BOOST_REQUIRE_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_DOS),
                            mainValidator.VerifySpend(tx, 0, emptyLock, 1000, MEMPOOL_HEIGHT, kSimpleBlockHeight, true).domain);
        const auto unconfirmed = RecordingSource::Make(1);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          mainValidator.VerifySpend(tx, 0, emptyLock, 1000, MEMPOOL_HEIGHT, kSimpleBlockHeight, true, std::nullopt, unconfirmed->GetToken()).domain);
        BOOST_CHECK_EQUAL(2u, unconfirmed->polls());

        BOOST_REQUIRE_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                            mainValidator.VerifySpend(tx, 5, emptyLock, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, true).domain);
        const auto badIndex = RecordingSource::Make(1);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          mainValidator.VerifySpend(tx, 5, emptyLock, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, true, std::nullopt, badIndex->GetToken()).domain);
        BOOST_CHECK_EQUAL(2u, badIndex->polls());
    }
    {
        // VerifyScript with one height for a two-input transaction: its argument
        // checks throw. A cancel that lands while deserialising wins.
        const std::vector<uint8_t> valid = ParseHex(kValidTxHex);
        const std::vector<int32_t> oneHeight{ kValidTxUtxoHeights[0] };
        BOOST_REQUIRE_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                            mainValidator.VerifyScript(valid, oneHeight, kValidTxBlockHeight, true).domain);
        const auto source = RecordingSource::Make(1);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          mainValidator.VerifyScript(valid, oneHeight, kValidTxBlockHeight, true, {}, source->GetToken()).domain);
        BOOST_CHECK_EQUAL(2u, source->polls());
    }

    bsv::CTxValidator validator(CBaseChainParams::REGTEST);
    validator.SetRequireStandard(true);
    {
        // A non-standard output: rejected by the output check right after deserialising.
        const std::vector<uint8_t> tx = PreGenesisTwoP2PKHInputsTx(CScript() << OP_TRUE);
        const auto verdict = validator.CheckStandardness(tx, kPreGenesisUtxoHeights2, 5000);
        BOOST_REQUIRE_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_DOS), verdict.domain);

        const auto source = RecordingSource::Make(1);  // cancel lands while deserialising
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          validator.CheckStandardness(tx, kPreGenesisUtxoHeights2, 5000, source->GetToken()).domain);
        BOOST_CHECK_EQUAL(2u, source->polls());
    }
    {
        // Standard, with two P2PKH inputs whose checks run no script.
        const std::vector<uint8_t> tx = PreGenesisTwoP2PKHInputsTx(P2PKHScript());
        BOOST_REQUIRE(bsv::TxErrorIsOk(validator.CheckStandardness(tx, kPreGenesisUtxoHeights2, 5000)));

        const auto source = RecordingSource::Make(2);  // cancel lands in the output check
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          validator.CheckStandardness(tx, kPreGenesisUtxoHeights2, 5000, source->GetToken()).domain);
        BOOST_CHECK_EQUAL(3u, source->polls());

        const auto second = RecordingSource::Make(3);  // cancel lands in input 0's check
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED),
                          validator.CheckStandardness(tx, kPreGenesisUtxoHeights2, 5000, second->GetToken()).domain);
        BOOST_CHECK_EQUAL(4u, second->polls());
    }
}

// CheckStandardness takes one UTXO height per input. Any other number, fewer or more,
// is an argument error, reported in the exception domain like the other entry points'
// size checks, by both overloads. That holds whether or not the policy reads the
// heights: pre-Genesis with standardness required (the per-input check reads them,
// so a short span must not be read past its end), and post-Genesis with the default
// policy, with standardness required (mainnet) or not (regtest), where they are never
// read.
BOOST_AUTO_TEST_CASE(check_standardness_requires_one_utxo_height_per_input)
{
    struct Case {
        const char* policy;
        bsv::CTxValidator validator;
        int32_t blockHeight;
        int32_t utxoHeight;
    };
    bsv::CTxValidator preGenesis(CBaseChainParams::REGTEST);
    preGenesis.SetRequireStandard(true);
    const std::vector<Case> cases{
        { "pre-Genesis, standardness required", preGenesis, 5000, 4000 },
        { "post-Genesis, mainnet default", bsv::CTxValidator(CBaseChainParams::MAIN), kSimpleBlockHeight, kSimpleUtxoHeights[0] },
        { "post-Genesis, regtest default (standardness not required)", bsv::CTxValidator(CBaseChainParams::REGTEST), kLongBlockHeight, kLongUtxoHeights[0] },
    };
    const std::vector<uint8_t> tx = PreGenesisTwoP2PKHInputsTx(P2PKHScript());  // two inputs

    for (const auto& c : cases) {
        BOOST_TEST_CONTEXT(c.policy) {
            const std::vector<int32_t> matching(2, c.utxoHeight);
            BOOST_REQUIRE(bsv::TxErrorIsOk(c.validator.CheckStandardness(tx, matching, c.blockHeight)));

            for (const size_t count : { size_t{ 0 }, size_t{ 1 }, size_t{ 3 } }) {
                const std::vector<int32_t> heights(count, c.utxoHeight);
                BOOST_TEST_CONTEXT("heights=" << count) {
                    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                                      c.validator.CheckStandardness(tx, heights, c.blockHeight).domain);
                    const auto token = task::CCancellationSource::Make()->GetToken();
                    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                                      c.validator.CheckStandardness(tx, heights, c.blockHeight, token).domain);
                }
            }
        }
    }
}

// All byte-parsing entry points require exactly one extended transaction.
BOOST_AUTO_TEST_CASE(check_standardness_rejects_trailing_bytes)
{
    const bsv::CTxValidator validator("main");
    const auto tx = ParseHex(kValidTxHex);
    const auto token = task::CCancellationSource::Make()->GetToken();
    BOOST_REQUIRE(bsv::TxErrorIsOk(validator.CheckStandardness(tx, kValidTxUtxoHeights, kValidTxBlockHeight)));
    BOOST_REQUIRE(bsv::TxErrorIsOk(validator.CheckStandardness(tx, kValidTxUtxoHeights, kValidTxBlockHeight, token)));

    for(const auto& suffix : { std::vector<uint8_t>{ 0 }, std::vector<uint8_t>{ 1, 2, 3 } })
    {
        auto trailing = tx;
        trailing.insert(trailing.end(), suffix.begin(), suffix.end());
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                          validator.CheckStandardness(trailing, kValidTxUtxoHeights, kValidTxBlockHeight).domain);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                          validator.CheckStandardness(trailing, kValidTxUtxoHeights, kValidTxBlockHeight, token).domain);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                          validator.VerifyScript(trailing, kValidTxUtxoHeights, kValidTxBlockHeight, true).domain);
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION),
                          validator.ValidateTransaction(trailing, kValidTxUtxoHeights, kValidTxBlockHeight, true).domain);
    }
}

// Cancellation after the entry poll wins over a suffix found after parsing.
BOOST_AUTO_TEST_CASE(post_deserialisation_cancel_precedes_trailing_bytes)
{
    const bsv::CTxValidator validator("main");
    auto extended = ParseHex(kValidTxHex);
    extended.push_back(0);
    auto plain = PlainEmptyScriptSigTx();
    plain.push_back(0);
    const std::vector<uint8_t> lockingScript{ OP_TRUE };

    const auto check = [&](const char* entry, const auto& call) {
        BOOST_TEST_CONTEXT(entry) {
            const auto liveToken = task::CCancellationSource::Make()->GetToken();
            BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION), call(liveToken).domain);
            // First poll answers false, then cancels: parsing starts with a live
            // result from the poll, but finishes with the source already cancelled.
            const auto source = RecordingSource::Make(1);
            BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), call(source->GetToken()).domain);
            BOOST_CHECK_EQUAL(2u, source->polls());
        }
    };
    for (const bool consensus : { true, false }) {
        BOOST_TEST_CONTEXT("consensus=" << consensus) {
            check("VerifyScript", [&](const auto& token) {
                return validator.VerifyScript(extended, kValidTxUtxoHeights, kValidTxBlockHeight, consensus, {}, token);
            });
            check("ValidateTransaction", [&](const auto& token) {
                return validator.ValidateTransaction(extended, kValidTxUtxoHeights, kValidTxBlockHeight, consensus, token);
            });
            check("VerifySpend", [&](const auto& token) {
                return validator.VerifySpend(plain, 0, lockingScript, 1000, kSimpleUtxoHeights[0], kSimpleBlockHeight, consensus, std::nullopt, token);
            });
        }
    }
    check("CheckStandardness", [&](const auto& token) {
        return validator.CheckStandardness(extended, kValidTxUtxoHeights, kValidTxBlockHeight, token);
    });
}

// VerifySpend passes the caller's token to the interpreter: a cancel from another
// thread while its script runs stops it, as for ValidateTransaction.
BOOST_AUTO_TEST_CASE(verify_spend_cancelled_mid_script_returns_cancelled_promptly)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("01"), 0);
    mtx.vin[0].scriptSig = CScript() << std::vector<uint8_t>(1 << 20, 0x01);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = Amount(1000);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << mtx;
    const std::vector<uint8_t> tx(ss.begin(), ss.end());
    CScript lock;
    for (size_t i = 0; i < 2000; ++i)
        lock << OP_DUP << OP_SHA256 << OP_DROP;
    const std::vector<uint8_t> lockBytes(lock.begin(), lock.end());
    const bsv::CTxValidator validator = LongRunningValidator();

    for (const bool consensus : { true, false }) {
        auto source = task::CCancellationSource::Make();
        const auto token = source->GetToken();
        const auto [result, latency] = RunAndCancelAfter(source, std::chrono::milliseconds(50), [&] {
            return validator.VerifySpend(tx, 0, lockBytes, 100000, kLongUtxoHeights[0], kLongBlockHeight, consensus, std::nullopt, token);
        });
        BOOST_TEST_CONTEXT("consensus=" << consensus) {
            BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), result.domain);
            BOOST_CHECK_LT(latency.count(), 1000);
        }
    }
}

// One signature hash over a large locking script is a single step with no poll
// inside. A cancel that lands during input 0's OP_CHECKSIG waits for that step, then
// stops at input 1's first poll: Cancelled, with input 1's OP_CHECKSIG never run. The source cancels itself right after the
// poll before input 0's OP_CHECKSIG, so the cancel always lands inside it.
BOOST_AUTO_TEST_CASE(cancelled_during_a_signature_hash_stops_at_the_next_input)
{
    const std::vector<uint8_t> tx = LargeSighashTx(20u << 20);
    const std::span<const uint8_t> etx(tx.data(), tx.size());
    const bsv::CTxValidator validator("main");

    // Polls of the consensus path: entry, after deserialising, after the output
    // checks, after the shared signature-hash data (4); then per input: the
    // signature push, and in the locking script the data push, OP_DROP, the pubkey
    // push and OP_CHECKSIG (5).
    constexpr uint64_t kPrefixPolls = 4;
    constexpr uint64_t kPollsPerInput = 5;
    constexpr uint64_t kInput0Checksig = kPrefixPolls + kPollsPerInput;   // poll before input 0's OP_CHECKSIG
    constexpr uint64_t kInput1First = kInput0Checksig + 1;                // input 1's first poll

    // Uncancelled run: the verdict, the poll count, and how long one OP_CHECKSIG takes.
    const auto reference = RecordingSource::Make();
    BOOST_REQUIRE(bsv::TxErrorIsOk(validator.ValidateTransaction(etx, kSighashUtxoHeights, kSighashBlockHeight, true, reference->GetToken())));
    BOOST_REQUIRE_EQUAL(kPrefixPolls + 2 * kPollsPerInput, reference->polls());
    const auto checksig = reference->pollTime(kInput1First) - reference->pollTime(kInput0Checksig);

    const auto source = RecordingSource::Make(kInput0Checksig);
    const TxError result = validator.ValidateTransaction(etx, kSighashUtxoHeights, kSighashBlockHeight, true, source->GetToken());
    const auto latency = RecordingSource::clock::now() - source->pollTime(kInput0Checksig);

    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    std::cerr << "[cancelled_during_a_signature_hash] checksig_us=" << duration_cast<microseconds>(checksig).count()
              << " latency_us=" << duration_cast<microseconds>(latency).count()
              << " polls=" << source->polls() << " domain=" << result.domain << '\n';
    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), result.domain);
    // Stopped at input 1's first poll: input 1's OP_CHECKSIG never ran. The latency
    // is printed, not asserted: it is one OP_CHECKSIG, and a loaded machine stretches
    // that step itself.
    BOOST_CHECK_EQUAL(kInput1First, source->polls());
}

// The flag-backed token is what the C bindings pass in: an int32_t the caller owns,
// non-zero meaning cancel, stored by any thread. Zero leaves the call alone, a store
// from another thread mid-script stops it, and a null flag never cancels.
BOOST_AUTO_TEST_CASE(flag_token_cancels_when_the_flag_is_set_from_another_thread)
{
    const std::vector<uint8_t> valid = ParseHex(kValidTxHex);
    const std::span<const uint8_t> validTx(valid.data(), valid.size());
    const bsv::CTxValidator mainValidator("main");

    alignas(4) int32_t zero = 0;
    BOOST_CHECK(bsv::TxErrorIsOk(mainValidator.ValidateTransaction(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true, bsv::MakeFlagCancellationToken(&zero))));
    BOOST_CHECK(bsv::TxErrorIsOk(mainValidator.ValidateTransaction(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true, bsv::MakeFlagCancellationToken(nullptr))));

    alignas(4) int32_t set = 1;
    BOOST_CHECK(IsCancelled(mainValidator.ValidateTransaction(validTx, kValidTxUtxoHeights, kValidTxBlockHeight, true, bsv::MakeFlagCancellationToken(&set))));

    const std::vector<uint8_t> tx = LongRunningTx(2000);
    const std::span<const uint8_t> etx(tx.data(), tx.size());
    const bsv::CTxValidator validator = LongRunningValidator();
    alignas(4) int32_t flag = 0;
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        StoreFlagRelaxed(&flag, 1);
    });
    const auto started = std::chrono::steady_clock::now();
    const TxError result = validator.ValidateTransaction(etx, kLongUtxoHeights, kLongBlockHeight, true, bsv::MakeFlagCancellationToken(&flag));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    canceller.join();
    BOOST_CHECK(IsCancelled(result));
    BOOST_CHECK_LT(elapsed.count(), 1000);
}

BOOST_AUTO_TEST_SUITE_END()
