#pragma once

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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

    // From the contents of proc/self/cgroup: the v1 hierarchy with the cpu
    // controller, else the v2 hierarchy (ID 0); nullopt when there is neither or
    // the file is malformed. Go 1.26 internal/runtime/cgroup/cgroup.go
    // parseCPUCgroup.
    std::optional<CpuCgroup> FindCpuCgroup(std::string_view cgroupFile);

    // From the contents of proc/self/mountinfo: the directory holding `cgroup`'s
    // files, under the first cgroup2 mount, or cgroup mount with the cpu super
    // option, whose root contains the cgroup; nullopt when there is none or the
    // file is malformed. Go 1.26 internal/runtime/cgroup/cgroup.go parseCPUMount.
    std::optional<std::string> FindCpuCgroupDir(std::string_view mountinfo,
                                                const CpuCgroup& cgroup);

    // From the contents of cgroup v2 cpu.max, "<quota> <period>\n": quota / period
    // in CPUs; nullopt for quota "max" or a malformed file. Go 1.26
    // internal/runtime/cgroup/cgroup.go parseV2Limit.
    std::optional<double> ParseV2Limit(std::string_view cpuMax);

    // From the contents of cgroup v1 cpu.cfs_quota_us and cpu.cfs_period_us, each
    // "<value>\n": quota / period in CPUs; nullopt for a negative quota or a
    // malformed file. Go 1.26 internal/runtime/cgroup/cgroup.go parseV1Number and
    // cgroup_linux.go ReadCPULimit.
    std::optional<double> ParseV1Limit(std::string_view quota, std::string_view period);

    // The CPU bandwidth limit (quota / period, in CPUs) of the cgroup this process
    // is in, read from the files under `root` ("/" on a live system): cgroup v2
    // cpu.max, or cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us of the hierarchy
    // with the cpu controller, located through the cgroup and mountinfo files of
    // proc/thread-self, else of proc/self. nullopt when there is no limit, or when
    // any file is missing, does not parse, or (a value file) is 64 bytes or more,
    // as Go reads those with one 64-byte pread. A zero or negative quota is a
    // limit, and a zero period gives +Inf, -Inf or NaN, as in Go.
    // A port of Go 1.26's internal/runtime/cgroup; cpu_limit.cpp lists the
    // differences.
    std::optional<double> CgroupCpuLimit(const std::filesystem::path& root);

    // Lowers a CPU count to a cgroup limit the way Go sets its default GOMAXPROCS
    // (runtime/cgroup_linux.go, adjustCgroupGOMAXPROCS, the same from go1.25.0 on):
    // the limit is rounded up to a whole CPU and to at least 2, and only ever
    // lowers `cpus`. -Inf gives 2; +Inf and NaN leave `cpus` unchanged.
    size_t LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit);

    // The CPU count EffectiveCpuCount returns, from what it read: the CPUs in the
    // affinity mask (nullopt where there is none or it cannot be read), else
    // `hardwareConcurrency`, lowered by LimitCpuCount to `cgroupLimit`; at least 1.
    size_t CpuCountFrom(std::optional<size_t> affinityCpus,
                        size_t hardwareConcurrency,
                        std::optional<double> cgroupLimit);

    // CPUs this process may use: on Linux the CPUs in the process's
    // sched_getaffinity mask (the main thread's, whichever thread calls; once the
    // main thread has exited, its mask as it was then), as Go counts them, else
    // hardware_concurrency(); lowered by LimitCpuCount to the cgroup limit read
    // under `cgroupRoot` ("/" on a live system; tests pass a temporary root). At
    // least 1. Reads the system on every call, so callers that need it often
    // should keep the result. Throws std::bad_alloc when out of memory, and
    // nothing else.
    size_t EffectiveCpuCount(const std::filesystem::path& cgroupRoot = "/");

} // namespace bsv::internal
