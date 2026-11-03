#pragma once

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.
//
// The cgroup parsing and the rounding follow Go 1.26's runtime and were checked
// against it with a differential test; cpu_limit.cpp lists the known
// differences.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace bsv::internal
{

    enum class CgroupVersion : uint8_t
    {
        V1,
        V2
    };

    // The cgroup whose CPU limit applies to this process.
    struct CpuCgroup
    {
        CgroupVersion version;
        std::string path; // relative to its hierarchy's root, starts with '/'
    };

    // Why a parser below rejected its input. Each is the error the Go function it
    // mirrors returns for the same input.
    enum class CgroupError : uint8_t
    {
        NoCgroup,       // ErrNoCgroup
        MalformedFile,  // errMalformedFile
        InvalidEscape,  // errInvalidEscape
        IncompleteLine, // errIncompleteLine
    };

    // A parser's result: the value, or why the input was rejected.
    template<typename T>
    using Parsed = std::variant<T, CgroupError>;

    // From the contents of proc/self/cgroup: the v1 hierarchy with the cpu
    // controller, else the v2 hierarchy (ID 0). NoCgroup when there is neither;
    // IncompleteLine for a line without its '\n', and MalformedFile for any other
    // malformed line, up to the v1 match. Mirrors Go's parseCPUCgroup.
    Parsed<CpuCgroup> FindCpuCgroup(std::string_view cgroupFile);

    // From the contents of proc/self/mountinfo: the directory holding `cgroup`'s
    // files, under the first cgroup2 mount, or cgroup mount with the cpu super
    // option, whose root contains the cgroup. NoCgroup when there is none;
    // InvalidEscape for a bad \ooo escape in a root that is unescaped or in the
    // selected mount point, and MalformedFile for a malformed line, up to the
    // match. Mirrors Go's parseCPUMount.
    Parsed<std::string> FindCpuCgroupDir(std::string_view mountinfo,
                                         const CpuCgroup& cgroup);

    // From the contents of cgroup v2 cpu.max, "<quota> <period>\n": quota / period
    // in CPUs; nullopt (no limit) for quota "max"; MalformedFile otherwise.
    // Mirrors Go's parseV2Limit.
    Parsed<std::optional<double>> ParseV2Limit(std::string_view cpuMax);

    // From the contents of cgroup v1 cpu.cfs_quota_us and cpu.cfs_period_us, each
    // "<value>\n": quota / period in CPUs; nullopt (no limit) for a negative
    // quota, whatever the period; MalformedFile otherwise. Mirrors Go's
    // parseV1Number and ReadCPULimit.
    Parsed<std::optional<double>> ParseV1Limit(std::string_view quota,
                                               std::string_view period);

    // The CPU bandwidth limit (quota / period, in CPUs) of the cgroup this process
    // is in, read from the files under `root` ("/" on a live system): cgroup v2
    // cpu.max, or cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us of the hierarchy
    // with the cpu controller, located through the cgroup and mountinfo files of
    // proc/self, or of proc/thread-self when proc/self/mountinfo does not read
    // (once the main thread has exited); both from the same directory. nullopt
    // when there is no limit, or when any file is missing, does not parse, or (a
    // value file) is 64 bytes or more. A zero or negative quota is a limit, and
    // a zero period gives +Inf, -Inf or NaN.
    std::optional<double> CgroupCpuLimit(const std::filesystem::path& root);

    // Lowers a CPU count to a cgroup limit: the limit is rounded up to a whole
    // CPU and to at least 2, and only ever lowers `cpus`. -Inf gives 2; +Inf and
    // NaN leave `cpus` unchanged. Mirrors Go's adjustCgroupGOMAXPROCS.
    size_t LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit);

    // The CPU count EffectiveCpuCount returns, from what it read: the CPUs in the
    // affinity mask (nullopt where there is none or it cannot be read), else
    // `hardwareConcurrency`, lowered by LimitCpuCount to `cgroupLimit`; at least 1.
    size_t CpuCountFrom(std::optional<size_t> affinityCpus,
                        size_t hardwareConcurrency,
                        std::optional<double> cgroupLimit);

    // CPUs this process may use: on Linux the CPUs in the process's
    // sched_getaffinity mask (the main thread's, whichever thread calls; once the
    // main thread has exited, its mask as it was then), else
    // hardware_concurrency(); lowered by LimitCpuCount to the cgroup limit read
    // under `cgroupRoot` ("/" on a live system; tests pass a temporary root):
    // the main thread's cgroup. Once the main thread has exited, the cgroup is
    // the calling thread's while the mask stays the main thread's, so a thread
    // moved to a cpu cgroup of its own then gets a count of its own. At least 1. Reads
    // the system on every call, so callers that need it often should keep the result.
    // Reading the mask allocates nothing; reading the cgroup files throws std::bad_alloc
    // when out of memory. Throws nothing else.
    size_t EffectiveCpuCount(const std::filesystem::path& cgroupRoot = "/");

} // namespace bsv::internal
