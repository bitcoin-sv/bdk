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
const std::string kValidTxHex = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000";
constexpr int32_t kValidTxBlockHeight = 632099;
const std::vector<int32_t> kValidTxUtxoHeights{ 631924, 631924 };

task::CCancellationToken CancelledToken()
{
    auto source = task::CCancellationSource::Make();
    source->Cancel();
    return source->GetToken();
}

bool IsCancelled(const TxError& r)
{
    return r.domain == static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED);
}

// Regtest heights at which the spent output and the block are post-Chronicle.
constexpr int32_t kLongBlockHeight = 20000;
const std::vector<int32_t> kLongUtxoHeights{ 19000 };

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

// A source a test cancels by hand, which counts its polls and records when each
// poll happened.
class RecordingSource final : public task::CCancellationSource {
public:
    using clock = std::chrono::steady_clock;
    static std::shared_ptr<RecordingSource> Make()
    {
        return std::shared_ptr<RecordingSource>(new RecordingSource());
    }
    bool IsCanceled() override
    {
        const uint64_t n = ++polls_;
        if (n < times_.size())
            times_[n] = clock::now();
        return CCancellationSource::IsCanceled();
    }
    uint64_t polls() const { return polls_; }
    clock::time_point pollTime(uint64_t n) const { return times_.at(n); }

private:
    RecordingSource() : times_(64) {}
    std::atomic<uint64_t> polls_{ 0 };
    std::vector<clock::time_point> times_;
};

// Two inputs, each spending <scriptCodeBytes push> OP_DROP <pubkey> OP_CHECKSIG with a
// FORKID signature: every OP_CHECKSIG hashes the whole locking script, a single step
// with no poll inside. Mainnet, post-Genesis.
constexpr int32_t kSighashBlockHeight = 800000;
const std::vector<int32_t> kSighashUtxoHeights{ 700000, 700000 };

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
                  << " latency_ms=" << latency.count() << std::endl;
        BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), result.domain);
        BOOST_CHECK_LT(latency.count(), 1000);
    }
}

// Every poll is a place the call can stop. For each k, a token that fires at poll
// k+1 must give exactly Cancelled when the uncancelled run polls more than k times,
// and the uncancelled result otherwise: a cancel never turns into a verdict, an
// exception or a different verdict. The cases cover a verdict of OK, a script
// failure, and policy mode running IsInputStandard on a P2SH input.
BOOST_AUTO_TEST_CASE(cancel_at_every_poll_gives_cancelled_or_the_uncancelled_result)
{
    struct Case {
        const char* name;
        std::vector<uint8_t> tx;
        std::vector<int32_t> utxoHeights;
        int32_t blockHeight;
        bool requireStandard;  // pre-Genesis, non-standard outputs are refused when true
    };
    std::vector<uint8_t> invalid = ParseHex(kValidTxHex);
    invalid[60] ^= 0x01;  // first input's signature
    const std::vector<Case> cases{
        { "valid", ParseHex(kValidTxHex), kValidTxUtxoHeights, kValidTxBlockHeight, false },
        { "bad-signature", invalid, kValidTxUtxoHeights, kValidTxBlockHeight, false },
        { "pre-genesis-p2sh", PreGenesisP2SHTx(), { 4000 }, 5000, true },
    };

    for (const auto& c : cases) {
        bsv::CTxValidator validator(c.name == std::string("pre-genesis-p2sh") ? CBaseChainParams::REGTEST : CBaseChainParams::MAIN);
        if (c.requireStandard)
            validator.SetRequireStandard(true);
        const std::span<const uint8_t> etx(c.tx.data(), c.tx.size());
        for (const bool consensus : { true, false }) {
            const auto counter = CountdownSource::Make(UINT64_MAX);
            const TxError expected = validator.ValidateTransaction(etx, c.utxoHeights, c.blockHeight, consensus, counter->GetToken());
            const uint64_t polls = counter->polls();
            std::cerr << "[cancel_at_every_poll] " << c.name << " consensus=" << consensus << " polls=" << polls
                      << " result=" << expected.domain << "/" << expected.code << std::endl;
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

// One signature hash over a large locking script is a single step with no poll
// inside. A cancel that lands during input 0's OP_CHECKSIG waits for that step, then
// stops at input 1's first poll: Cancelled, with a latency of about one OP_CHECKSIG,
// not two.
BOOST_AUTO_TEST_CASE(cancelled_during_a_signature_hash_stops_at_the_next_input)
{
    const std::vector<uint8_t> tx = LargeSighashTx(20u << 20);
    const std::span<const uint8_t> etx(tx.data(), tx.size());
    const bsv::CTxValidator validator("main");

    // Polls of the consensus path: entry, after deserialising, after the output
    // checks, before the scripts (4); then per input: the signature push, and in the
    // locking script the data push, OP_DROP, the pubkey push and OP_CHECKSIG (5).
    constexpr uint64_t kPrefixPolls = 4;
    constexpr uint64_t kPollsPerInput = 5;
    constexpr uint64_t kInput0Checksig = kPrefixPolls + kPollsPerInput;   // poll before input 0's OP_CHECKSIG
    constexpr uint64_t kInput1First = kInput0Checksig + 1;                // input 1's first poll

    // Uncancelled run: the verdict, the poll count, and how long one OP_CHECKSIG takes.
    const auto reference = RecordingSource::Make();
    BOOST_REQUIRE(bsv::TxErrorIsOk(validator.ValidateTransaction(etx, kSighashUtxoHeights, kSighashBlockHeight, true, reference->GetToken())));
    BOOST_REQUIRE_EQUAL(kPrefixPolls + 2 * kPollsPerInput, reference->polls());
    const auto checksig = reference->pollTime(kInput1First) - reference->pollTime(kInput0Checksig);

    const auto source = RecordingSource::Make();
    std::atomic<int64_t> cancelledAtNs{ 0 };
    std::thread canceller([&] {
        while (source->polls() < kInput0Checksig)
            std::this_thread::yield();
        std::this_thread::sleep_for(checksig / 4);
        cancelledAtNs = RecordingSource::clock::now().time_since_epoch().count();
        source->Cancel();
    });
    const TxError result = validator.ValidateTransaction(etx, kSighashUtxoHeights, kSighashBlockHeight, true, source->GetToken());
    const auto returnedAt = RecordingSource::clock::now();
    canceller.join();
    const auto latency = returnedAt - RecordingSource::clock::time_point(RecordingSource::clock::duration(cancelledAtNs.load()));

    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    std::cerr << "[cancelled_during_a_signature_hash] checksig_us=" << duration_cast<microseconds>(checksig).count()
              << " latency_us=" << duration_cast<microseconds>(latency).count()
              << " polls=" << source->polls() << " domain=" << result.domain << std::endl;
    BOOST_CHECK_EQUAL(static_cast<int32_t>(TX_ERR_DOMAIN_CANCELLED), result.domain);
    BOOST_CHECK_EQUAL(kInput1First, source->polls());
    BOOST_CHECK_LT(duration_cast<microseconds>(latency).count(), duration_cast<microseconds>(checksig).count() + 20000);
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
        std::atomic_ref<int32_t>(flag).store(1, std::memory_order_relaxed);
    });
    const auto started = std::chrono::steady_clock::now();
    const TxError result = validator.ValidateTransaction(etx, kLongUtxoHeights, kLongBlockHeight, true, bsv::MakeFlagCancellationToken(&flag));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    canceller.join();
    BOOST_CHECK(IsCancelled(result));
    BOOST_CHECK_LT(elapsed.count(), 1000);
}

BOOST_AUTO_TEST_SUITE_END()
