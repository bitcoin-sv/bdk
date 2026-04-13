/// Benchmark for VerifyScriptBatchParallel with threads pinned to physical cores.
/// Each thread in the pool is pinned to a distinct physical core via sched_setaffinity,
/// avoiding hyperthreading contention on compute-bound ECDSA workloads.

#include <cassert>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <boost/program_options.hpp>

#include "extendedTx.hpp"
#include "assembler.h"
#include "utilstrencodings.h"
#include "scriptengine.hpp"
#include "verifyarg.hpp"
#include "thread_pool.hpp"

namespace po = boost::program_options;

// TXID: d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7
// Block: 620940 — P2PKH, 1 input, 2 outputs, 192 bytes
const std::string CHAIN_NET    = "main";
const int32_t     BLOCK_HEIGHT = 620940;
const std::string TX_ID        = "d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7";
const std::string TX_HEX_EXTENDED =
    "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7"
    "000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda"
    "0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b0"
    "5ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437"
    "fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140"
    "ff0246afa795658812b42788ac00000000";
const std::vector<int32_t> UTXO_HEIGHTS = {574441};

int main(int argc, char* argv[])
{
    int  iterations      = 100;
    int  batchSize       = 10000;
    int  numThreadsArg   = 0;   // 0 = auto (all physical cores)
    bool disableConsensus = false;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h",   "produce help message")
        ("iterations,i",  po::value<int>(&iterations)->default_value(100),
            "number of benchmark iterations")
        ("batch-size,b",  po::value<int>(&batchSize)->default_value(10000),
            "transactions per batch")
        ("threads,t",     po::value<int>(&numThreadsArg)->default_value(0),
            "number of threads to use (0 = all physical cores)")
        ("disable-consensus,c", po::bool_switch(&disableConsensus)->default_value(false),
            "disable consensus mode");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) { std::cout << desc << "\n"; return 0; }
        po::notify(vm);
    } catch (po::error& e) {
        std::cerr << "Error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }

    // Detect topology for display and default thread count
    const auto   physicalCores = bsv::getPhysicalCoreIds();
    const size_t numPhysical   = physicalCores.size();
    const size_t logicalCpus   = std::thread::hardware_concurrency();
    const size_t threadsToUse  = (numThreadsArg > 0)
                                     ? static_cast<size_t>(numThreadsArg)
                                     : numPhysical;

    std::cout << "System Information:\n";
    std::cout << "  Logical CPUs:    " << logicalCpus   << "\n";
    std::cout << "  Physical cores:  " << numPhysical   << "\n";
    std::cout << "  Threads to use:  " << threadsToUse
              << (numThreadsArg == 0 ? " (auto: all physical cores)" : " (user specified)")
              << "\n";
    std::cout << "\n";

    std::cout << "Benchmark Configuration:\n";
    std::cout << "  Network:         " << CHAIN_NET        << "\n";
    std::cout << "  TXID:            " << TX_ID            << "\n";
    std::cout << "  Block Height:    " << BLOCK_HEIGHT     << "\n";
    std::cout << "  Tx size:         " << TX_HEX_EXTENDED.length() / 2 << " bytes\n";
    std::cout << "  Batch size:      " << batchSize        << " transactions\n";
    std::cout << "  Iterations:      " << iterations       << "\n";
    std::cout << "  Consensus:       " << (disableConsensus ? "disabled" : "enabled") << "\n";
    std::cout << "\n";

    const std::vector<uint8_t>    txBin      = ParseHex(TX_HEX_EXTENDED);
    const std::span<const int32_t> utxoHeights(UTXO_HEIGHTS);
    const bool consensus = !disableConsensus;

    // Engine construction — thread pool is lazy, created on first parallel call
    std::cout << "Initializing ScriptEngine...\n";
    const bsv::CScriptEngine se(CHAIN_NET);

    // Pre-construct the batch outside of timing
    std::cout << "Pre-constructing batch of " << batchSize << " transactions...\n\n";
    bsv::VerifyBatch batch;
    batch.reserve(batchSize);
    for (int j = 0; j < batchSize; ++j)
        batch.add(bsv::VerifyArg(txBin, utxoHeights, BLOCK_HEIGHT, consensus));

    // Warmup — also triggers thread pool creation (pinning messages appear here)
    std::cout << "Running warmup (thread pool initializes here)...\n";
    for (int i = 0; i < 5; ++i) {
        const auto results = se.VerifyScriptBatchParallel(batch, threadsToUse);
        for (const auto& r : results) {
            if (r != SCRIPT_ERR_OK) {
                std::cerr << "ERROR: warmup verification failed\n";
                return 1;
            }
        }
    }

    std::cout << "\nRunning benchmark...\n\n";

    std::chrono::nanoseconds totalDuration(0);
    std::chrono::nanoseconds minDuration = std::chrono::nanoseconds::max();
    std::chrono::nanoseconds maxDuration(0);

    for (int i = 0; i < iterations; ++i) {
        auto start   = std::chrono::high_resolution_clock::now();
        const auto results = se.VerifyScriptBatchParallel(batch, threadsToUse);
        auto end     = std::chrono::high_resolution_clock::now();

        for (size_t j = 0; j < results.size(); ++j) {
            if (results[j] != SCRIPT_ERR_OK) {
                std::cerr << "ERROR: verification failed at iteration " << i
                          << ", tx " << j << "\n";
                return 1;
            }
        }

        const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        totalDuration += duration;
        if (duration < minDuration) minDuration = duration;
        if (duration > maxDuration) maxDuration = duration;
    }

    const double totalSeconds = totalDuration.count() / 1e9;
    const double avgNs        = static_cast<double>(totalDuration.count()) / iterations;
    const double avgPerTxNs   = avgNs / batchSize;
    const long long totalTx   = static_cast<long long>(iterations) * batchSize;

    std::cout << "Benchmark Results:\n";
    std::cout << "==================\n";
    std::cout << std::fixed;
    std::cout << "Threads used:      " << threadsToUse << " (pinned to physical cores)\n";
    std::cout << std::setprecision(4);
    std::cout << "Total time:        " << totalSeconds << " seconds\n";
    std::cout << std::setprecision(2);
    std::cout << "Average per batch: " << avgNs / 1e6 << " ms\n";
    std::cout << "Average per tx:    " << avgPerTxNs / 1000.0 << " µs ("
              << std::setprecision(0) << avgPerTxNs << " ns)\n";
    std::cout << "Min batch:         " << std::setprecision(2) << minDuration.count() / 1e6 << " ms\n";
    std::cout << "Max batch:         " << maxDuration.count() / 1e6 << " ms\n";
    std::cout << "Throughput:        " << std::setprecision(0) << totalTx / totalSeconds << " tx/sec\n";

    return 0;
}
