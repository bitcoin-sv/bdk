#include <txvalidator.hpp>
#include <utilstrencodings.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* VALID_EXTENDED_TX =
    "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000";

constexpr int32_t UTXO_HEIGHT = 574441;
constexpr int32_t BLOCK_HEIGHT = 620940;

int positiveArgument(const char* value, const char* name) {
    const long parsed = std::strtol(value, nullptr, 10);
    if(parsed <= 0 || parsed > 100000000L) {
        throw std::invalid_argument(std::string{name} + " must be between 1 and 100000000");
    }
    return static_cast<int>(parsed);
}

double percentile(const std::vector<double>& sorted, double fraction) {
    const auto index = static_cast<size_t>((sorted.size() - 1) * fraction);
    return sorted[index];
}

} // namespace

int main(int argc, char** argv) try {
    const int iterations = argc > 1 ? positiveArgument(argv[1], "iterations") : 1000;
    const int samples = argc > 2 ? positiveArgument(argv[2], "samples") : 9;
    const int warmupIterations = std::max(20, iterations / 10);

    const std::vector<uint8_t> extendedTx = ParseHex(VALID_EXTENDED_TX);
    const std::vector<int32_t> utxoHeights{UTXO_HEIGHT};
    const std::span<const uint32_t> customFlags{};
    const bsv::CTxValidator validator{"main"};

    const auto verifyOnce = [&] {
        return validator.VerifyScript(
            extendedTx,
            utxoHeights,
            BLOCK_HEIGHT,
            true,
            customFlags);
    };

    const TxError initial = verifyOnce();
    if(initial.domain != TX_ERR_DOMAIN_OK || initial.code != 0) {
        std::cerr << "known-valid vector failed: domain=" << initial.domain
                  << " code=" << initial.code << '\n';
        return 2;
    }

    volatile int32_t resultGuard = 0;
    for(int i = 0; i < warmupIterations; ++i) {
        const TxError result = verifyOnce();
        resultGuard = resultGuard ^ result.domain ^ result.code;
    }

    std::vector<double> microsPerOperation;
    microsPerOperation.reserve(static_cast<size_t>(samples));
    for(int sample = 0; sample < samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for(int i = 0; i < iterations; ++i) {
            const TxError result = verifyOnce();
            resultGuard = resultGuard ^ result.domain ^ result.code;
        }
        const auto end = std::chrono::steady_clock::now();
        const double elapsedMicros =
            std::chrono::duration<double, std::micro>(end - start).count();
        microsPerOperation.push_back(elapsedMicros / iterations);
    }

    std::sort(microsPerOperation.begin(), microsPerOperation.end());
    const double median = percentile(microsPerOperation, 0.5);
    const double p95 = percentile(microsPerOperation, 0.95);
    const double mean = std::accumulate(
        microsPerOperation.begin(), microsPerOperation.end(), 0.0) /
        microsPerOperation.size();

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"benchmark\": \"BDK native VerifyScript\",\n"
              << "  \"vector\": \"mainnet-p2pkh-block-620940\",\n"
              << "  \"iterationsPerSample\": " << iterations << ",\n"
              << "  \"samples\": " << samples << ",\n"
              << "  \"medianMicrosPerOperation\": " << median << ",\n"
              << "  \"p95MicrosPerOperation\": " << p95 << ",\n"
              << "  \"meanMicrosPerOperation\": " << mean << ",\n"
              << "  \"medianOperationsPerSecond\": " << (1000000.0 / median) << ",\n"
              << "  \"resultGuard\": " << resultGuard << "\n"
              << "}\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
