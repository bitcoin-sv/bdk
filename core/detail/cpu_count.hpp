#pragma once

// The CPUs this process may use, from its affinity mask and cgroup.

#include <detail/cgroup.hpp>

#include <cstddef>
#include <filesystem>

namespace bsv::detail
{
    // The CPUs this process may use, at least 1: those in its affinity mask,
    // lowered to its cgroup's CPU limit when that can be read. `cgroupRoot` is
    // where the proc and cgroup files are found ("/" on a live system).
    size_t EffectiveCpuCount(const std::filesystem::path& cgroupRoot = "/");

    // `availableCpus` lowered to the CPUs `bandwidth` needs, but not below 2;
    // never raised, so unchanged when `bandwidth` is unlimited.
    size_t LimitedCpuCount(size_t availableCpus, const CpuBandwidth& bandwidth);
}
