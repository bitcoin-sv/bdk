// Serial vs parallel input verification numbers, for choosing and checking
// the parallel input threshold and the helper pool's behaviour under
// concurrent callers. A benchmark, not a test: it is not registered with
// ctest and asserts only that every tx validates. Build and run it alone on an
// otherwise idle machine, about 45 s:
//   cmake --build <build> --target bench_txvalidator_inputs
//   bench_txvalidator_inputs [-- --input-pool-cores=N]
// CI runs it in an optional step (workflow_dispatch input run-benchmarks).
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE bench_txvalidator_inputs
#else
#define BOOST_TEST_MODULE bench_txvalidator_inputsd
#endif
#include <boost/test/unit_test.hpp>

#include "key.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "script/sighashtype.h"
#include "streams.h"
#include "version.h"

#include "extendedTx.hpp"
#include "internal/txvalidator_internal.hpp"
#include "txerror.h"
#include "txvalidator.hpp"

// `-- --input-pool-cores=N` sizes the input pool to at most N cores.
struct InputPoolCoresFromArgs {
    InputPoolCoresFromArgs()
    {
        const std::string cores = "--input-pool-cores=";
        const auto& suite = boost::unit_test::framework::master_test_suite();
        for (int i = 1; i < suite.argc; ++i) {
            const std::string arg = suite.argv[i];
            if (arg.rfind(cores, 0) == 0)
                bsv::detail::SetInputPoolCoresForTesting(std::stoull(arg.substr(cores.size())));
        }
    }
};
BOOST_TEST_GLOBAL_FIXTURE(InputPoolCoresFromArgs);

namespace {

constexpr int64_t P2PKH_INPUT_SATOSHIS = 100000;
constexpr int64_t P2PKH_OUTPUT_SATOSHIS = 1000;
constexpr int32_t P2PKH_BLOCK_HEIGHT = 700000;  // post-Genesis, pre-Chronicle on mainnet
constexpr int32_t P2PKH_UTXO_HEIGHT = 699999;
constexpr size_t FORCE_SERIAL = std::numeric_limits<size_t>::max();

// Sets the parallel input threshold for a scope and restores it after.
struct InputThresholdGuard {
    explicit InputThresholdGuard(size_t minInputs)
        : previous(bsv::detail::SetParallelInputThresholdForTesting(minInputs)) {}
    ~InputThresholdGuard() { bsv::detail::SetParallelInputThresholdForTesting(previous); }
    InputThresholdGuard(const InputThresholdGuard&) = delete;
    InputThresholdGuard& operator=(const InputThresholdGuard&) = delete;
    const size_t previous;
};

CKey MakeKey(uint8_t tag, size_t index)
{
    std::array<uint8_t, 32> raw{};
    raw[29] = tag;
    raw[30] = static_cast<uint8_t>(index >> 8);
    raw[31] = static_cast<uint8_t>(index);
    CKey key;
    key.Set(raw.begin(), raw.end(), /*fCompressedIn=*/true);
    return key;
}

// An extended tx of `nInputs` P2PKH spends of one key, each signed
// SIGHASH_ALL|FORKID, paying one output back to that key.
bsv::CMutableTransactionExtended MakeP2PKHSpend(size_t nInputs)
{
    const CKey key = MakeKey(0x64, 1);
    const CPubKey pubkey = key.GetPubKey();
    const CScript locking = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID())
                                      << OP_EQUALVERIFY << OP_CHECKSIG;
    const SigHashType allForkId = SigHashType().withForkId();
    bsv::CMutableTransactionExtended eTx;
    eTx.mtx.nVersion = 2;
    eTx.mtx.vin.resize(nInputs);
    eTx.vutxo.resize(nInputs);
    for (size_t i = 0; i < nInputs; ++i) {
        eTx.mtx.vin[i].prevout = COutPoint(uint256S("01"), static_cast<uint32_t>(i));
        eTx.vutxo[i].nValue = Amount(P2PKH_INPUT_SATOSHIS);
        eTx.vutxo[i].scriptPubKey = locking;
    }
    eTx.mtx.vout.resize(1);
    eTx.mtx.vout[0].nValue = Amount(P2PKH_OUTPUT_SATOSHIS);
    eTx.mtx.vout[0].scriptPubKey = locking;
    const CTransaction unsignedTx(eTx.mtx);
    const PrecomputedTransactionData txdata(unsignedTx);
    for (size_t i = 0; i < nInputs; ++i) {
        const uint256 sighash = SignatureHash(locking, unsignedTx, static_cast<unsigned int>(i), allForkId,
            eTx.vutxo[i].nValue, &txdata);
        std::vector<uint8_t> sig;
        BOOST_REQUIRE(key.Sign(sighash, sig));
        sig.push_back(static_cast<uint8_t>(allForkId.getRawSigHashType()));
        eTx.mtx.vin[i].scriptSig = CScript() << sig << ToByteVector(pubkey);
    }
    return eTx;
}

std::vector<uint8_t> SerializeExtended(const bsv::CMutableTransactionExtended& eTx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx;
    return { ss.begin(), ss.end() };
}

constexpr size_t DEEP_IF_DEPTH = 100000;
constexpr size_t DEEP_MULTISIG_KEYS = 1000;

// Inputs (0) P2PKH, (1) OP_IF nested `depth` deep, (2) a bare 1-of-`nKeys`
// multisig signed by the key CHECKMULTISIG reaches last: the deepest scripts
// of test_txvalidator's deep_scripts_verify_on_a_helper. Verified in
// consensus mode (no post-Genesis size, op or key-count limits).
bsv::CMutableTransactionExtended MakeDeepScriptSpend(size_t depth, size_t nKeys)
{
    auto eTx = MakeP2PKHSpend(3);
    CScript nested;
    for (size_t i = 0; i < depth; ++i)
        nested << OP_1 << OP_IF;
    nested << OP_1;
    for (size_t i = 0; i < depth; ++i)
        nested << OP_ENDIF;
    eTx.vutxo[1].scriptPubKey = nested;
    eTx.mtx.vin[1].scriptSig = CScript();

    std::vector<CKey> keys;
    keys.reserve(nKeys);
    CScript multisig = CScript() << OP_1;
    for (size_t k = 0; k < nKeys; ++k) {
        keys.push_back(MakeKey(0x6b, k + 1));
        multisig << ToByteVector(keys.back().GetPubKey());
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

} // namespace

BOOST_AUTO_TEST_SUITE(parallel_input_benchmark)

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
    return { SerializeExtended(MakeP2PKHSpend(nInputs)), std::vector<int32_t>(nInputs, P2PKH_UTXO_HEIGHT), nInputs };
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
    std::cerr << "[threshold_sweep] hardware_concurrency=" << std::thread::hardware_concurrency() << '\n';
    std::cerr << "[threshold_sweep] inputs serial_us parallel_us speedup" << '\n';
    for (const size_t n : { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 256, 512, 1024 }) {
        const BenchTx tx = MakeBenchTx(n);
        const double parallel = BestUsPerTx(se, tx, n, 7);
        const double serial = BestUsPerTx(se, tx, FORCE_SERIAL, 7);
        std::cerr << "[threshold_sweep] " << n << " " << serial << " " << parallel << " " << serial / parallel << '\n';
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
              << " default_threshold=" << defaultThreshold << '\n';
    std::cerr << "[large_tx_serial_vs_parallel] inputs serial_ms parallel_ms speedup" << '\n';
    for (const size_t n : { 1000, 10000, 40000 }) {
        const BenchTx tx = MakeBenchTx(n);
        const double serial = BestUsPerTx(se, tx, FORCE_SERIAL, 3) / 1000.0;
        const double parallel = BestUsPerTx(se, tx, defaultThreshold, 3) / 1000.0;
        std::cerr << "[large_tx_serial_vs_parallel] " << n << " " << serial << " " << parallel << " "
                  << serial / parallel << '\n';
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
    callers.reserve(perCaller.size());
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
              << " default_threshold=" << defaultThreshold << '\n';
    std::cerr << "[concurrent_callers_throughput] scenario | serial_inputs_per_s | parallel_inputs_per_s | ratio" << '\n';
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
                  << " | " << parallel / serial << '\n';
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
    std::cerr << "[core_budget_under_concurrent_callers] pool_cores=" << cores << '\n';
    std::cerr << "[core_budget_under_concurrent_callers] callers | peak_busy | samples_over_max(cores,callers) | samples_over_cores" << '\n';
    for (const size_t callerCount : { size_t{ 1 }, size_t{ 2 }, size_t{ 3 }, cores, 2 * cores }) {
        std::atomic<bool> stop{ false };
        std::atomic<size_t> failures{ 0 };
        std::vector<std::thread> callers;
        callers.reserve(callerCount);
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
                  << overCallers << "/" << samples << " | " << overCores << "/" << samples << '\n';
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
    for (volatile unsigned char& byte : area)
        byte = 0xa5;
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
    const auto eTx = MakeDeepScriptSpend(DEEP_IF_DEPTH, DEEP_MULTISIG_KEYS);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << eTx.mtx;
    const std::vector<uint8_t> txBin(ss.begin(), ss.end());
    std::cerr << "[stack_use_of_the_deepest_scripts] input | peak_stack_bytes (probe " << STACK_PROBE_BYTES << ")" << '\n';
    PaintStack();
    UseStack<65536>();
    std::cerr << "[stack_use_of_the_deepest_scripts] calibration, a 65536-byte frame | "
              << STACK_PROBE_BYTES - UnusedStackBytes() << '\n';
    for (const auto& [index, name] : { std::pair{ size_t{ 0 }, "P2PKH" }, std::pair{ size_t{ 1 }, "OP_IF nested 100000 deep" },
                                       std::pair{ size_t{ 2 }, "1-of-1000 multisig, 1000 checks" } }) {
        const CTxOut& source = eTx.vutxo[index];
        PaintStack();
        const TxError status = se.VerifySpend(std::span<const uint8_t>(txBin), static_cast<uint32_t>(index),
            std::span<const uint8_t>(source.scriptPubKey.data(), source.scriptPubKey.size()),
            source.nValue.GetSatoshis(), P2PKH_UTXO_HEIGHT, P2PKH_BLOCK_HEIGHT, /*consensus=*/true);
        const size_t used = STACK_PROBE_BYTES - UnusedStackBytes();
        BOOST_REQUIRE(bsv::TxErrorIsOk(status));
        std::cerr << "[stack_use_of_the_deepest_scripts] " << name << " | " << used << '\n';
    }
}

BOOST_AUTO_TEST_SUITE_END()
