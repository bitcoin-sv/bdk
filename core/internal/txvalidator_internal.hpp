#ifndef __TXVALIDATOR_INTERNAL_HPP__
#define __TXVALIDATOR_INTERNAL_HPP__

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bsv::detail {

// Sets the input count from which a transaction's inputs are verified on
// several threads, process-wide, and returns the previous value.
// SIZE_MAX forces the serial path; 2 forces the parallel path for every
// multi-input transaction. Tests only.
size_t SetParallelInputThresholdForTesting(size_t minInputs);

// Number of inputs verified on pool helper threads since process start. Tests only.
uint64_t HelperVerifiedInputCountForTesting();

// The CPU bandwidth limit (quota / period, in CPUs) of the cgroup this process
// is in, read from the files under `root` ("/" on a live system): cgroup v2
// cpu.max, or cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us of the hierarchy
// with the cpu controller, located through proc/self/cgroup and
// proc/self/mountinfo. nullopt when there is no limit, or when any file is
// missing or does not parse. Follows Go's internal/runtime/cgroup.
std::optional<double> CgroupCpuLimit(const std::string& root);

// Lowers a CPU count to a cgroup limit the way Go 1.25 sets its default
// GOMAXPROCS (runtime/cgroup_linux.go, adjustCgroupGOMAXPROCS): the limit is
// rounded up to a whole CPU and to at least 2, and only ever lowers `cpus`.
size_t LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit);

// CPUs this process may use: hardware_concurrency(), lowered on Linux to the
// sched_getaffinity mask and to the cgroup limit by LimitCpuCount. Reads the
// system on every call; the input pool calls it once.
size_t EffectiveCpuCount();

} // namespace bsv::detail

#endif /* __TXVALIDATOR_INTERNAL_HPP__ */
