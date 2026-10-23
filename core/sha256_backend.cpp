#include "sha256_backend.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>

#include "crypto/sha256_stream_scalar.h"

// Which hardware transforms this target has, from the compiler's own target
// macros, so a universal, Rosetta or cross build decides per target:
//   BDK_SHA256_X86_HW : SV Node's SSE4 and SHA-NI transforms and its
//                       sha256_dispatch::AutoDetect() (core/CMakeLists.txt,
//                       sha256_x86_shani.cpp)
//   BDK_SHA256_ARM_HW : the ARMv8 SHA-2 transforms (sha256_arm_shani_target.cpp)
// Neither for the wasm variant or MSVC-compatible compilers (including clang-cl).
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER) && !defined(__EMSCRIPTEN__)
#if defined(__x86_64__) || defined(__amd64__)
#define BDK_SHA256_X86_HW 1 // NOLINT(cppcoreguidelines-macro-usage)
#elif defined(__aarch64__)
#define BDK_SHA256_ARM_HW 1
#endif
#endif

#if defined(BDK_SHA256_X86_HW)
#include <cpuid.h>
#include "crypto/sha256_shani.h"
#include "crypto/sha256_sse4.h"
#include "crypto/sha256d64_shani.h"
#endif

#if defined(BDK_SHA256_ARM_HW)
#if defined(__linux__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

// Raw-pointer entry points of the vendored Bitcoin Core code in sha256_arm_shani.cpp
namespace sha256_arm_shani
{
    void Transform(uint32_t* s, const unsigned char* chunk, size_t blocks);
}
namespace sha256d64_arm_shani
{
    void Transform_2way(unsigned char* output, const unsigned char* input);
}
#endif

namespace
{
#if defined(BDK_SHA256_ARM_HW)
    void ArmStream(std::span<const std::uint8_t> ip, std::span<std::uint32_t, 8> op)
    {
        sha256_arm_shani::Transform(op.data(), ip.data(), ip.size_bytes() / 64);
    }

    void ArmDBlock2way(std::span<const std::uint8_t, 2 * sha256_dispatch::d_pair_bytes> ip,
                       std::span<std::uint8_t, 2 * sha256_dispatch::hash_bytes> op)
    {
        sha256d64_arm_shani::Transform_2way(op.data(), ip.data());
    }

    // Asks the OS rather than executing an SHA-2 instruction, so a CPU without
    // the extension never runs one
    bool HaveArmSha2()
    {
#if defined(__linux__)
        return (getauxval(AT_HWCAP) & HWCAP_SHA2) != 0;
#elif defined(__APPLE__)
        int val{0};
        size_t len{sizeof(val)};
        return sysctlbyname("hw.optional.arm.FEAT_SHA256", &val, &len, nullptr, 0) == 0 && val != 0;
#else
        return false; // no detection on this OS: stay scalar
#endif
    }
#endif

#if defined(BDK_SHA256_X86_HW)
    bool HaveSse41()
    {
        uint32_t eax{}, ebx{}, ecx{}, edx{};
        return __get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 && ((ecx >> 19) & 1u) != 0;
    }

    bool HaveShani()
    {
        uint32_t eax{}, ebx{}, ecx{}, edx{};
        return __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0 && ((ebx >> 29) & 1u) != 0;
    }
#endif

    // Same vectors as SV Node's sha256_dispatch.cpp self-test: no input, the
    // padded empty string, and 64 spaces padded to two blocks. Each input has a
    // one-byte prefix so the transform sees unaligned data.
    [[maybe_unused]] bool SelfTestStream(sha256_dispatch::StreamType tr)
    {
        static const std::array<unsigned char, 65> in1 = {0, 0x80};
        static const std::array<unsigned char, 129> in2 = {
            0,  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,   32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,   32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,   32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0x80, 0,  0,
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,
            0,  0,  0,  0,  0,  0,  0,  0,  2,  0};
        static const std::array<uint32_t, 8> init = {
            0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
            0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul};
        static const std::array<uint32_t, 8> out1 = {
            0xe3b0c442ul, 0x98fc1c14ul, 0x9afbf4c8ul, 0x996fb924ul,
            0x27ae41e4ul, 0x649b934cul, 0xa495991bul, 0x7852b855ul};
        static const std::array<uint32_t, 8> out2 = {
            0xce4153b0ul, 0x147c2a86ul, 0x3ed4298eul, 0xe0676bc8ul,
            0x79fc77a1ul, 0x2abe1f49ul, 0xb2b055dful, 0x1069523eul};

        std::array<uint32_t, 8> buf = init;
        tr(std::span<const std::uint8_t>{}, buf);
        if(buf != init)
            return false;
        tr(std::span{in1}.subspan<1>(), buf);
        if(buf != out1)
            return false;
        buf = init;
        tr(std::span{in2}.subspan<1>(), buf);
        return buf == out2;
    }

    // Same vector as SV Node's SelfTestD64 (from Bitcoin Core): two 64-byte
    // pairs at a one-byte offset, double-SHA-256 of each
    [[maybe_unused]] bool SelfTestDBlock2way(sha256_dispatch::DBlock2wayType tr)
    {
        static constexpr std::array<const std::uint8_t, 130> data{
            "-"
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
            "eiusmod tempor incididunt ut labore et dolore magna aliqua. Et m"};
        static constexpr std::array<unsigned char, 64> expected = {
            0x09, 0x3a, 0xc4, 0xd0, 0x0f, 0xf7, 0x57, 0xe1, 0x72, 0x85, 0x79, 0x42, 0xfe, 0xe7, 0xe0, 0xa0,
            0xfc, 0x52, 0xd7, 0xdb, 0x07, 0x63, 0x45, 0xfb, 0x53, 0x14, 0x7d, 0x17, 0x22, 0x86, 0xf0, 0x52,
            0x48, 0xb6, 0x11, 0x9e, 0x6e, 0x48, 0x81, 0x6d, 0xcc, 0x57, 0x1f, 0xb2, 0x97, 0xa8, 0xd5, 0x25,
            0x9b, 0x82, 0xaa, 0x89, 0xe2, 0xfd, 0x2d, 0x56, 0xe8, 0x28, 0x83, 0x0b, 0xe2, 0xfa, 0x53, 0xb7,
        };
        std::array<std::uint8_t, 64> out{};
        tr(std::span{data}.subspan<1, 128>(), std::span{out});
        return out == expected;
    }

    [[maybe_unused]] void AddIfSelfTested(std::vector<bsv::Sha256Backend>& backends,
                                          const char* name,
                                          sha256_dispatch::StreamType stream,
                                          sha256_dispatch::DBlock2wayType d_block_2way)
    {
        if(!SelfTestStream(stream))
            return;
        if(d_block_2way != nullptr && !SelfTestDBlock2way(d_block_2way))
            d_block_2way = nullptr;
        backends.push_back({name, stream, d_block_2way});
    }

    [[maybe_unused]] bool ForceScalar()
    {
        const char* v = std::getenv("BDK_SHA256_FORCE_SCALAR");
        return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
    }
}

const std::vector<bsv::Sha256Backend>& bsv::Sha256SupportedBackends()
{
    static const std::vector<Sha256Backend> backends = [] {
        std::vector<Sha256Backend> ret{{"standard", sha256_stream_scalar::Transform, nullptr}};
#if defined(BDK_SHA256_X86_HW)
        // The order and conditions match SV Node's AutoDetect()
        if(HaveSse41())
        {
            AddIfSelfTested(ret, "sse4", sha256_sse4::Transform, nullptr);
            if(HaveShani())
                AddIfSelfTested(ret, "shani", sha256_shani::Transform, sha256d64_shani::Transform_2way);
        }
#endif
#if defined(BDK_SHA256_ARM_HW)
        if(HaveArmSha2())
            AddIfSelfTested(ret, "arm_shani", ArmStream, ArmDBlock2way);
#endif
        return ret;
    }();
    return backends;
}

const std::string& bsv::SelectSha256Backend()
{
#if defined(__EMSCRIPTEN__)
    // Scalar only, and kept minimal: the wasm bundles have a size ceiling
    static const std::string selected{"standard"};
#else
    // Never destroyed: the Go and Rust getters hand out pointers into it, and a
    // thread may still read one while the process exits
    static const std::string& selected = *new std::string([] () -> std::string {
        if(ForceScalar())
            return "standard(forced)";
#if defined(BDK_SHA256_X86_HW)
        // SV Node's own dispatcher installs SSE4 / SHA-NI by CPUID
        return sha256_dispatch::AutoDetect();
#else
        const Sha256Backend& best = Sha256SupportedBackends().back();
        sha256_dispatch::stream = best.stream;
        std::string ret{best.name};
        if(best.d_block_2way != nullptr)
        {
            sha256_dispatch::d_block_2way = best.d_block_2way;
            ret += std::string{","} + best.name + "(2way)";
        }
        return ret;
#endif
    }());
#endif
    return selected;
}
