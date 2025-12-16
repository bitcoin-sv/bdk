/// Benchmark program for se.VerifyScript and se.VerifyScriptBatch with hardcoded transaction data

#include <cassert>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

#include <boost/program_options.hpp>

#include "extendedTx.hpp"
#include "assembler.h"
#include "utilstrencodings.h"
#include "scriptengine.hpp"
#include "verifyarg.hpp"

namespace po = boost::program_options;

// Hardcoded transaction data from CSV:
// TXID: d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7
// Block: 620940
// Size: 192 bytes (1 input, 1 output, no OP_RETURN)
const std::string CHAIN_NET = "main";
const int32_t BLOCK_HEIGHT = 620940;
const std::string TX_ID = "d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7";
const std::string TX_HEX_EXTENDED = "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000";
const std::vector<int32_t> UTXO_HEIGHTS = {574441};

////////////////////////////////////////////////////////////////////////////////
// Main benchmark program
int main(int argc, char* argv[])
{
    int iterations = 10000;
    int batchSize = 1000;
    bool disableConsensus = false;
    bool benchBatch = false;

    // Define and parse the command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("iterations,i", po::value<int>(&iterations)->default_value(10000), "number of iterations to run")
        ("batch-size,b", po::value<int>(&batchSize)->default_value(1000), "batch size (number of transactions per batch)")
        ("disable-consensus,c", po::bool_switch(&disableConsensus)->default_value(false), "disable the consensus")
        ("bench-batch", po::bool_switch(&benchBatch)->default_value(false), "benchmark batch processing instead of single");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        po::notify(vm);
    }
    catch (po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << desc << std::endl;
        return 1;
    }

    std::cout << "Benchmark Configuration:" << std::endl;
    std::cout << "  Network: " << CHAIN_NET << std::endl;
    std::cout << "  TXID: " << TX_ID << std::endl;
    std::cout << "  Block Height: " << BLOCK_HEIGHT << std::endl;
    std::cout << "  Transaction Size: " << TX_HEX_EXTENDED.length() / 2 << " bytes" << std::endl;
    std::cout << "  Mode: " << (benchBatch ? "Batch" : "Single") << std::endl;
    if (benchBatch) {
        std::cout << "  Batch Size: " << batchSize << " transactions" << std::endl;
    }
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Consensus: " << (disableConsensus ? "disabled" : "enabled") << std::endl;
    std::cout << std::endl;

    // Preparse the transaction binary
    const std::vector<uint8_t> txBinExtended = ParseHex(TX_HEX_EXTENDED);
    const std::span<const int32_t> utxoHeights(UTXO_HEIGHTS);
    const bool consensus = !disableConsensus;

    // Initialize script engine
    const bsv::CScriptEngine se(CHAIN_NET);

    // Warmup run (not measured)
    std::cout << "Running warmup..." << std::endl;

    if (benchBatch) {
        // Warmup for batch mode
        for (int i = 0; i < 10; ++i) {
            bsv::VerifyBatch batch;
            batch.reserve(batchSize);
            for (int j = 0; j < batchSize; ++j) {
                batch.add(bsv::VerifyArg(txBinExtended, utxoHeights, BLOCK_HEIGHT, consensus));
            }
            const auto results = se.VerifyScriptBatch(batch);
            if (results.size() != static_cast<size_t>(batchSize)) {
                std::cerr << "ERROR: VerifyScriptBatch returned wrong number of results during warmup" << std::endl;
                return 1;
            }
            for (const auto& ret : results) {
                if (ret != SCRIPT_ERR_OK) {
                    std::cerr << "ERROR: VerifyScriptBatch failed during warmup" << std::endl;
                    return 1;
                }
            }
        }
    } else {
        // Warmup for single mode
        for (int i = 0; i < 100; ++i) {
            const ScriptError ret = se.VerifyScript(txBinExtended, utxoHeights, BLOCK_HEIGHT, consensus);
            if (ret != SCRIPT_ERR_OK) {
                std::cerr << "ERROR: VerifyScript failed during warmup" << std::endl;
                return 1;
            }
        }
    }

    // Actual benchmark
    std::cout << "Running benchmark..." << std::endl;

    std::chrono::nanoseconds totalDuration(0);
    std::chrono::nanoseconds minDuration = std::chrono::nanoseconds::max();
    std::chrono::nanoseconds maxDuration(0);

    if (benchBatch) {
        // Benchmark batch mode
        for (int i = 0; i < iterations; ++i) {
            bsv::VerifyBatch batch;
            batch.reserve(batchSize);
            for (int j = 0; j < batchSize; ++j) {
                batch.add(bsv::VerifyArg(txBinExtended, utxoHeights, BLOCK_HEIGHT, consensus));
            }

            auto start = std::chrono::high_resolution_clock::now();
            const auto results = se.VerifyScriptBatch(batch);
            auto end = std::chrono::high_resolution_clock::now();

            if (results.size() != static_cast<size_t>(batchSize)) {
                std::cerr << "ERROR: VerifyScriptBatch returned wrong number of results at iteration " << i << std::endl;
                return 1;
            }

            for (size_t j = 0; j < results.size(); ++j) {
                if (results[j] != SCRIPT_ERR_OK) {
                    std::cerr << "ERROR: VerifyScriptBatch failed at iteration " << i << ", item " << j << std::endl;
                    return 1;
                }
            }

            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            totalDuration += duration;

            if (duration < minDuration) {
                minDuration = duration;
            }
            if (duration > maxDuration) {
                maxDuration = duration;
            }
        }
    } else {
        // Benchmark single mode
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            const ScriptError ret = se.VerifyScript(txBinExtended, utxoHeights, BLOCK_HEIGHT, consensus);
            auto end = std::chrono::high_resolution_clock::now();

            if (ret != SCRIPT_ERR_OK) {
                std::cerr << "ERROR: VerifyScript failed at iteration " << i << std::endl;
                return 1;
            }

            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            totalDuration += duration;

            if (duration < minDuration) {
                minDuration = duration;
            }
            if (duration > maxDuration) {
                maxDuration = duration;
            }
        }
    }

    // Calculate statistics
    double avgNanoseconds = static_cast<double>(totalDuration.count()) / iterations;
    double avgMicroseconds = avgNanoseconds / 1000.0;
    double totalSeconds = totalDuration.count() / 1e9;

    std::cout << std::endl;
    std::cout << "Benchmark Results:" << std::endl;
    std::cout << "==================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time:        " << std::setprecision(4) << totalSeconds << " seconds" << std::endl;
    if (benchBatch) {
        std::cout << "Average per batch: " << std::setprecision(2) << avgMicroseconds / 1000.0 << " ms ("
                  << std::setprecision(0) << avgNanoseconds << " ns)" << std::endl;
        double avgPerTx = avgNanoseconds / batchSize;
        std::cout << "Average per tx:    " << std::setprecision(2) << avgPerTx / 1000.0 << " µs ("
                  << std::setprecision(0) << avgPerTx << " ns)" << std::endl;
    } else {
        std::cout << "Average per call:  " << std::setprecision(2) << avgMicroseconds << " µs ("
                  << std::setprecision(0) << avgNanoseconds << " ns)" << std::endl;
    }
    std::cout << "Min:               " << std::setprecision(2) << minDuration.count() / 1000.0 << " µs ("
              << minDuration.count() << " ns)" << std::endl;
    std::cout << "Max:               " << std::setprecision(2) << maxDuration.count() / 1000.0 << " µs ("
              << maxDuration.count() << " ns)" << std::endl;

    if (benchBatch) {
        int totalTx = iterations * batchSize;
        std::cout << "Throughput:        " << std::setprecision(0) << (totalTx / totalSeconds) << " tx/sec" << std::endl;
    } else {
        std::cout << "Throughput:        " << std::setprecision(0) << (iterations / totalSeconds) << " calls/sec" << std::endl;
    }

    return 0;
}
