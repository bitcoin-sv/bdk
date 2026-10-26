#pragma once

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bsv::detail {

// A whole-string base-10 integer from a cgroup file, no surrounding
// whitespace; nullopt otherwise, or when it does not fit.
std::optional<int64_t> ParseCgroupInt(std::string_view text);

// The CPU bandwidth limit (quota / period, in CPUs) of the cgroup this process
// is in, read from the files under `root` ("/" on a live system): cgroup v2
// cpu.max, or cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us of the hierarchy
// with the cpu controller, located through proc/self/cgroup and
// proc/self/mountinfo. nullopt when there is no limit, or when any file is
// missing or does not parse. The lookup follows Go 1.26's
// internal/runtime/cgroup (FindCPUCgroup, FindCPUMountPoint).
std::optional<double> CgroupCpuLimit(const std::string& root);

// Lowers a CPU count to a cgroup limit the way Go sets its default GOMAXPROCS
// (runtime/cgroup_linux.go, adjustCgroupGOMAXPROCS, the same from go1.25.0 on):
// the limit is rounded up to a whole CPU and to at least 2, and only ever
// lowers `cpus`.
size_t LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit);

// CPUs this process may use: hardware_concurrency(), lowered on Linux to the
// process's sched_getaffinity mask (the main thread's, whichever thread
// calls) and, by LimitCpuCount, to the cgroup limit read under `cgroupRoot`
// ("/" on a live system; tests pass a fixture root). Reads the system on every call; the
// input pool calls it once, again only after failing to allocate or to start
// a helper.
size_t EffectiveCpuCount(const std::string& cgroupRoot = "/");

} // namespace bsv::detail
