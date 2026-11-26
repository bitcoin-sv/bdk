#include <detail/cpu_count.hpp>

#include <detail/cgroup.hpp>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <optional>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#endif

using namespace bsv::detail;

// The fewest CPUs a cgroup limit lowers the count to. One would leave the
// pool no parallelism, and a quota under one CPU usually marks a bursty
// workload, which can use more for short spells.
constexpr size_t MIN_CGROUP_CPUS = 2;

// The CPUs in the main thread's affinity mask, whichever thread calls;
// nullopt off Linux or when the mask can't be read.
#if defined(__linux__)
static std::optional<size_t> AffinityCpuCount()
{
    // Above nr_cpu_ids, which CONFIG_NR_CPUS caps at 8192 on x86-64 and 4096
    // on arm64.
    constexpr size_t MAX_AFFINITY_CPUS = 65'536;
    std::array<cpu_set_t, MAX_AFFINITY_CPUS / CPU_SETSIZE> mask{};
    static_assert(sizeof(mask) * CHAR_BIT == MAX_AFFINITY_CPUS);
    if(sched_getaffinity(getpid(), sizeof(mask), mask.data()) != 0)
        return std::nullopt;

    const int count{std::accumulate(mask.begin(),
                                    mask.end(),
                                    0,
                                    [](const int sum, const cpu_set_t& set)
                                    { return sum + CPU_COUNT(&set); })};
    if(count <= 0)
        return std::nullopt;

    return static_cast<size_t>(count);
}

#else
static std::optional<size_t> AffinityCpuCount() { return std::nullopt; }
#endif

size_t bsv::detail::LimitedCpuCount(const size_t availableCpus,
                                    const CpuBandwidth& bandwidth)
{
    if(bandwidth.IsUnlimited())
        return availableCpus;

    const auto cgroupCpus{std::max<uint64_t>(bandwidth.CpusNeeded(), MIN_CGROUP_CPUS)};
    return static_cast<size_t>(std::min<uint64_t>(cgroupCpus, availableCpus));
}

// Once the main thread has exited, its mask no longer changes, but
// FindCgroupDir follows the calling thread's cgroup: the count can pair a
// stale mask with a current limit. The cgroup is read on every platform, as
// off Linux its files are absent and no limit applies.
size_t bsv::detail::EffectiveCpuCount(const std::filesystem::path& cgroupRoot)
{
    const size_t availableCpus{
        AffinityCpuCount().value_or(std::max(std::thread::hardware_concurrency(), 1U))};
    return FindCgroupDir(cgroupRoot)
        .and_then(ReadCpuBandwidth)
        .transform([availableCpus](const CpuBandwidth& bandwidth)
                   { return LimitedCpuCount(availableCpus, bandwidth); })
        .value_or(availableCpus);
}
