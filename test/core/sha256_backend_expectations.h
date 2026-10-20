#ifndef __SHA256_BACKEND_EXPECTATIONS_H__
#define __SHA256_BACKEND_EXPECTATIONS_H__

// What the SHA-256 backend selection should produce on this machine, for the
// test suites. Include after <boost/test/unit_test.hpp>.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "crypto/sha256_dispatch.h"
#include "sha256_backend.hpp"

#if defined(__x86_64__) || defined(__amd64__)
#include <cpuid.h>
#endif

namespace sha256_test
{
    // The fastest backend this CPU supports, found without BDK's own detection
    // code: CPUID on x86-64, /proc/cpuinfo on Linux ARM. On Apple silicon it is
    // a fixed expectation rather than a probe: every Apple silicon core
    // implements FEAT_SHA256.
    inline std::string ExpectedBestBackend()
    {
#if defined(__x86_64__) || defined(__amd64__)
        uint32_t eax{}, ebx{}, ecx{}, edx{};
        const bool sse41 = __get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 && ((ecx >> 19) & 1u) != 0;
        const bool shani = __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0 && ((ebx >> 29) & 1u) != 0;
        if(sse41 && shani)
            return "shani";
        if(sse41)
            return "sse4";
        return "standard";
#elif defined(__aarch64__) && defined(__APPLE__)
        return "arm_shani";
#elif defined(__aarch64__) && defined(__linux__)
        std::ifstream cpuinfo{"/proc/cpuinfo"};
        std::string line;
        while(std::getline(cpuinfo, line))
        {
            if(line.rfind("Features", 0) != 0)
                continue;
            std::istringstream features{line.substr(line.find(':') + 1)};
            std::string feature;
            while(features >> feature)
                if(feature == "sha2")
                    return "arm_shani";
            return "standard";
        }
        return "standard";
#else
        return "standard";
#endif
    }

    inline bool ForcedScalar()
    {
        const char* v = std::getenv("BDK_SHA256_FORCE_SCALAR");
        return v != nullptr && *v != '\0' && std::string{v} != "0";
    }

    inline bool HasTwoWay(const std::string& backend)
    {
        return backend == "shani" || backend == "arm_shani";
    }

    // The full string bsv::SelectSha256Backend() must return. `forced`: whether
    // BDK_SHA256_FORCE_SCALAR was set when the process started.
    inline std::string ExpectedSelection(bool forced = ForcedScalar())
    {
        if(forced)
            return "standard(forced)";
        const std::string best = ExpectedBestBackend();
        return HasTwoWay(best) ? best + "," + best + "(2way)" : best;
    }

    // Checks the transforms actually installed in the sha256_dispatch pointers
    // against what this CPU should run. It never calls SelectSha256Backend(),
    // so it can check whether something else made the selection.
    inline void CheckInstalledBackend(bool forced = ForcedScalar())
    {
        if(forced)
        {
            BOOST_TEST(sha256_dispatch::stream == sha256_stream_scalar::Transform);
            BOOST_TEST(sha256_dispatch::d_block_2way == nullptr);
            return;
        }

        const std::string best = ExpectedBestBackend();
        const auto& supported = bsv::Sha256SupportedBackends();
        const auto it = std::find_if(supported.begin(), supported.end(),
                                     [&](const bsv::Sha256Backend& b) { return best == b.name; });
        BOOST_REQUIRE_MESSAGE(it != supported.end(),
                              "the CPU supports " << best << " but BDK does not offer it");
        BOOST_TEST((it == std::prev(supported.end())), best << " is not BDK's fastest backend");
        BOOST_TEST(sha256_dispatch::stream == it->stream, "installed stream is not " << best);
        if(HasTwoWay(best))
        {
            BOOST_TEST(it->d_block_2way != nullptr, best << " lost its 2-way transform");
            BOOST_TEST(sha256_dispatch::d_block_2way == it->d_block_2way, "installed 2-way is not " << best);
        }
        else
        {
            BOOST_TEST(sha256_dispatch::d_block_2way == nullptr);
        }
    }
}

#endif /* __SHA256_BACKEND_EXPECTATIONS_H__ */
