// CPUs available to this process, for sizing the input verification pool.
// The cgroup lookup is a port of Go 1.26's internal/runtime/cgroup: cgroup.go
// parseCPUCgroup, parseCPUMount, hasPathPrefix, unescapePath, parseV1Number
// and parseV2Limit, and cgroup_linux.go FindCPU and ReadCPULimit. It matches
// the mount whose root contains the cgroup, strips that root, skips paths
// outside the cgroup namespace and takes the first match; Go 1.25 ignored the
// mount root and kept the last cgroup2 mount. The rounding is Go's default
// GOMAXPROCS rule (runtime/cgroup_linux.go adjustCgroupGOMAXPROCS), unchanged
// since go1.25.0. With GOMAXPROCS unset and Go 1.26 or later, BDK and the Go
// runtime in one process derive the same CPU count from the same files.
// FindCpuCgroup, FindCpuCgroupDir, ParseV2Limit and ParseV1Limit each port one
// Go function and take file contents; CgroupCpuLimit only reads the files.
// Known differences:
// - a limit of +Inf or NaN (a zero period; the kernel's minimum is 1 ms) or
//   above 2^31 CPUs: Go converts it to int32, which is platform-specific; BDK
//   leaves the count unchanged, as such a limit could not lower it anyway.
// - Go's fixed buffers: lines over 64 KiB and paths over PATH_MAX, which the
//   kernel does not write.
// - a mount point with stray backslashes, which Go's parser indexes out of
//   range on; the kernel writes '\' as \134.
// - once the main thread has exited, proc/self/mountinfo no longer opens; BDK
//   then reads the cgroup and mountinfo files of proc/thread-self, the calling
//   thread's (see ReadProcFiles).
// Go also re-reads the limit and the affinity mask periodically; BDK reads
// them on every EffectiveCpuCount call.

#include <internal/cpu_limit.hpp>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <utilstrencodings.h>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sched.h>
#include <unistd.h>
#endif

static std::optional<std::string> ReadFile(const std::filesystem::path& path)
{
    const std::ifstream in(path, std::ios::binary);
    if(!in)
        return std::nullopt;
    std::ostringstream content;
    // A read error (EISDIR, EIO) ends the copy and sets failbit on `content`,
    // not on `in`. So does copying nothing: an empty file is no data either,
    // which every parser rejects anyway.
    content << in.rdbuf();
    if(!content)
        return std::nullopt;
    return content.str();
}

static std::vector<std::string_view> Split(std::string_view text, char sep)
{
    std::vector<std::string_view> parts;
    size_t start = 0;
    for(;;)
    {
        const size_t end = text.find(sep, start);
        parts.push_back(text.substr(start,
                                    end == std::string_view::npos ? std::string_view::npos
                                                                  : end - start));
        if(end == std::string_view::npos)
            return parts;
        start = end + 1;
    }
}

namespace
{

    struct Line
    {
        std::string_view text; // without its '\n'
        bool complete;         // false for a last line with no '\n'
    };

} // namespace

// The lines of a /proc file as Go's lineReader (line_reader.go) returns them:
// nothing after a final '\n', and a last line without one is incomplete.
static std::vector<Line> Lines(std::string_view content)
{
    std::vector<Line> lines;
    while(!content.empty())
    {
        const size_t newline = content.find('\n');
        if(newline == std::string_view::npos)
        {
            lines.push_back(Line{.text = content, .complete = false});
            break;
        }
        lines.push_back(Line{.text = content.substr(0, newline), .complete = true});
        content.remove_prefix(newline + 1);
    }
    return lines;
}

// The single line of a cgroup file, without its newline.
static std::optional<std::string_view> SingleLine(std::string_view content)
{
    const size_t newline = content.find('\n');
    if(newline == std::string_view::npos)
        return std::nullopt;
    return content.substr(0, newline);
}

static bool ContainsCpu(std::string_view commaSeparated)
{
    return std::ranges::any_of(Split(commaSeparated, ','),
                               [](std::string_view item) { return item == "cpu"; });
}

// cgroup path `path` is `root` or below it. `root` starts with '/'.
static bool HasPathPrefix(std::string_view path, std::string_view root)
{
    if(root.size() == 1)
        return true;
    return path.starts_with(root)
        && (path.size() == root.size() || path[root.size()] == '/');
}

// mountinfo escapes space, tab, newline and backslash as \ooo.
static std::optional<std::string> UnescapeMountPath(std::string_view text)
{
    constexpr int MAX_ESCAPED_BYTE = 0xFF;
    std::string out;
    for(size_t i = 0; i < text.size(); ++i)
    {
        if(text[i] != '\\')
        {
            out.push_back(text[i]);
            continue;
        }
        if(i + 3 >= text.size())
            return std::nullopt;
        int value = 0;
        for(size_t k = 1; k <= 3; ++k)
        {
            const char c = text[i + k];
            if(c < '0' || c > '7')
                return std::nullopt;
            value = (value * 8) + (c - '0');
        }
        if(value > MAX_ESCAPED_BYTE)
            return std::nullopt;
        out.push_back(static_cast<char>(value));
        i += 3;
    }
    return out;
}

using bsv::internal::CgroupVersion;
using bsv::internal::CpuCgroup;

// Go 1.26 internal/runtime/cgroup/cgroup.go parseCPUCgroup.
// Lines are "hierarchy-ID:controller-list:cgroup-path". Any line up to the v1
// match that is malformed or has no '\n' rejects the file.
std::optional<CpuCgroup> bsv::internal::FindCpuCgroup(std::string_view cgroupFile)
{
    std::optional<CpuCgroup> v2;
    for(const auto& [line, complete] : Lines(cgroupFile))
    {
        if(!complete)
            return std::nullopt;
        const size_t first = line.find(':');
        const size_t second = first == std::string_view::npos ? first
                                                              : line.find(':', first + 1);
        if(second == std::string_view::npos)
            return std::nullopt;
        const std::string_view hierarchy = line.substr(0, first);
        const std::string_view controllers = line.substr(first + 1, second - first - 1);
        const std::string_view path = line.substr(second + 1);
        if(path.empty() || path[0] != '/')
            return std::nullopt;
        if(hierarchy == "0")
            v2 = CpuCgroup{.version = CgroupVersion::V2, .path = std::string(path)};
        else if(ContainsCpu(controllers))
            return CpuCgroup{.version = CgroupVersion::V1, .path = std::string(path)};
    }
    return v2;
}

// Go 1.26 internal/runtime/cgroup/cgroup.go parseCPUMount, which also builds
// the directory. A line is
//   ID parent-ID major:minor root mount-point options [optional...] - type source
//   super-options
// Every line up to the match needs a root starting with '/', a " - "
// separator and a type followed by a space; a cgroup mount also needs its
// source. Roots are unescaped on mounts of the wanted type, and the mount
// point on the match only.
std::optional<std::string> bsv::internal::FindCpuCgroupDir(std::string_view mountinfo,
                                                           const CpuCgroup& cgroup)
{
    for(const auto& entry : Lines(mountinfo))
    {
        std::string_view line = entry.text;
        // The field before the next space, which the line then starts after.
        const auto next = [&line]() -> std::optional<std::string_view>
        {
            const size_t space = line.find(' ');
            if(space == std::string_view::npos)
                return std::nullopt;
            const std::string_view field = line.substr(0, space);
            line.remove_prefix(space + 1);
            return field;
        };
        if(!next() || !next() || !next()) // ID, parent ID, major:minor
            return std::nullopt;
        const auto root = next();
        if(!root || root->empty() || (*root)[0] != '/')
            return std::nullopt;
        const auto mountPoint = next();
        if(!mountPoint)
            return std::nullopt;
        for(;;)
        { // options and optional fields, up to " - "
            const size_t space = line.find(' ');
            if(space == std::string_view::npos || space + 3 >= line.size())
                return std::nullopt;
            if(line.substr(space, 3) == " - ")
            {
                line.remove_prefix(space + 3);
                break;
            }
            line.remove_prefix(space + 1);
        }
        const auto fsType = next();
        if(!fsType)
            return std::nullopt;
        if(cgroup.version == CgroupVersion::V1)
        {
            if(*fsType != "cgroup")
                continue;
            if(!next()) // source
                return std::nullopt;
            if(!ContainsCpu(line)) // super options, the rest of the line
                continue;
        }
        else if(*fsType != "cgroup2")
        {
            continue;
        }

        const auto unescapedRoot = UnescapeMountPath(*root);
        if(!unescapedRoot)
            return std::nullopt;
        if(!HasPathPrefix(cgroup.path, *unescapedRoot))
            continue;
        std::string_view rel = std::string_view(cgroup.path)
                                   .substr(unescapedRoot->size());
        if(unescapedRoot->size() == 1 && cgroup.path.size() > 1)
            rel = cgroup.path;
        if(HasPathPrefix(rel, "/.."))
            continue; // outside this cgroup namespace's view of the mount
        const auto unescapedMountPoint = UnescapeMountPath(*mountPoint);
        if(!unescapedMountPoint)
            return std::nullopt;
        // Concatenated, as Go builds it: the matching above is Go's byte-wise,
        // lexical one, and the result is the same bytes.
        return *unescapedMountPoint + std::string(rel);
    }
    return std::nullopt;
}

// quota / period in CPUs, as Go's float64 division gives it: a zero or
// negative quota is a limit, which the rounding raises to 2, and a zero period
// gives +Inf, -Inf or NaN (spelled out here, as dividing by zero is undefined
// in C++).
static double QuotaPerPeriod(int64_t quota, int64_t period)
{
    if(period == 0)
    {
        if(quota == 0)
            return std::numeric_limits<double>::quiet_NaN();
        return quota > 0 ? std::numeric_limits<double>::infinity()
                         : -std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(quota) / static_cast<double>(period);
}

// Go 1.26 internal/runtime/cgroup/cgroup.go parseV2Limit. Go's integers are
// strconv.ParseInt(s, 10, 64): base 10, an optional '+' or '-', the whole
// string, int64 range. SV Node's ParseInt64 accepts exactly the same strings.
std::optional<double> bsv::internal::ParseV2Limit(std::string_view cpuMax)
{
    const size_t space = cpuMax.find(' ');
    if(space == std::string_view::npos)
        return std::nullopt;
    if(cpuMax.substr(0, space) == "max")
        return std::nullopt;
    const auto periodLine = SingleLine(cpuMax.substr(space + 1));
    if(!periodLine)
        return std::nullopt;
    int64_t quota{};
    int64_t period{};
    if(!ParseInt64(std::string(cpuMax.substr(0, space)), &quota)
       || !ParseInt64(std::string(*periodLine), &period))
        return std::nullopt;
    return QuotaPerPeriod(quota, period);
}

// Go 1.26 internal/runtime/cgroup/cgroup.go parseV1Number: cpu.cfs_quota_us
// or cpu.cfs_period_us, "<value>\n".
static std::optional<int64_t> ParseV1Number(std::string_view content)
{
    const auto line = SingleLine(content);
    int64_t value{};
    if(!line || !ParseInt64(std::string(*line), &value))
        return std::nullopt;
    return value;
}

// Go 1.26 internal/runtime/cgroup/cgroup_linux.go ReadCPULimit, cgroup v1.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the two files, named as in Go
std::optional<double> bsv::internal::ParseV1Limit(std::string_view quota,
                                                  std::string_view period)
{
    const auto quotaValue = ParseV1Number(quota);
    if(!quotaValue || *quotaValue < 0)
        return std::nullopt; // negative, usually -1: no limit
    const auto periodValue = ParseV1Number(period);
    if(!periodValue)
        return std::nullopt;
    return QuotaPerPeriod(*quotaValue, *periodValue);
}

// A cgroup value file. Go reads one with a single 64-byte pread and rejects it
// if that fills the buffer.
static std::optional<std::string> ReadValueFile(const std::filesystem::path& path)
{
    constexpr size_t MAX_VALUE_FILE_SIZE = 63;
    auto content = ReadFile(path);
    if(content && content->size() > MAX_VALUE_FILE_SIZE)
        return std::nullopt;
    return content;
}

namespace
{

    struct ProcFiles
    {
        std::string cgroup;
        std::string mountinfo;
    };

} // namespace

// The cgroup and mountinfo files, both from proc/self, the main thread's, as
// Go reads them; or both from proc/thread-self, the calling thread's, when
// proc/self/mountinfo does not read. That happens once the main thread has
// exited: proc/self is then that exited thread's, and its mountinfo no longer
// opens (EINVAL, as exit_task_namespaces has cleared its nsproxy, which
// mounts_open_common requires). On a cgroup v1 hierarchy its cgroup then reads
// as "/" (proc_cgroup_show prints that for an exiting task), so the cgroup
// file comes from the same directory as mountinfo, never from the other one.
// Go never meets this, as it reads both files at startup. thread-self's files
// are the process's unless the calling thread has its own mount namespace or
// was moved to another cgroup on its own. Kernels before 3.17 have no
// thread-self.
static std::optional<ProcFiles> ReadProcFiles(const std::filesystem::path& root)
{
    for(const std::string_view dir : {"proc/self", "proc/thread-self"})
    {
        auto mountinfo = ReadFile(root / dir / "mountinfo");
        if(!mountinfo)
            continue;
        auto cgroup = ReadFile(root / dir / "cgroup");
        if(!cgroup)
            return std::nullopt;
        return ProcFiles{std::move(*cgroup), std::move(*mountinfo)};
    }
    return std::nullopt;
}

std::optional<double> bsv::internal::CgroupCpuLimit(const std::filesystem::path& root)
{
    const auto files = ReadProcFiles(root);
    if(!files)
        return std::nullopt;
    const auto cgroup = FindCpuCgroup(files->cgroup);
    if(!cgroup)
        return std::nullopt;
    const auto dir = FindCpuCgroupDir(files->mountinfo, *cgroup);
    if(!dir)
        return std::nullopt;
    // `dir` is absolute; operator/ would let it replace `root`.
    const std::filesystem::path cgroupDir = root
                                          / std::filesystem::path(*dir).relative_path();

    if(cgroup->version == CgroupVersion::V2)
    {
        const auto cpuMax = ReadValueFile(cgroupDir / "cpu.max");
        return cpuMax ? ParseV2Limit(*cpuMax) : std::nullopt;
    }
    const auto quota = ReadValueFile(cgroupDir / "cpu.cfs_quota_us");
    const auto period = ReadValueFile(cgroupDir / "cpu.cfs_period_us");
    if(!quota || !period)
        return std::nullopt;
    return ParseV1Limit(*quota, *period);
}

size_t bsv::internal::LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit)
{
    if(!cgroupLimit)
        return cpus;
    const double limit = std::max(std::ceil(*cgroupLimit), 2.0);
    return limit < static_cast<double>(cpus) ? static_cast<size_t>(limit) : cpus;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
// CPUs in the process's affinity mask. getpid() names the main thread (its
// thread ID is the process ID), so every caller reads the main thread's mask,
// whichever thread it runs on. Go reads the same mask: runtime/os_linux.go
// getCPUCount calls sched_getaffinity(0) from the main thread at startup
// (osinit), into a fixed buffer of the same size. Once the main thread has
// exited, its mask no longer changes: cpuset updates skip exiting tasks
// (cpuset_update_tasks_cpumask), so the count is the mask as it was then.
// The kernel fails the call (EINVAL) only for a buffer with fewer bits than
// nr_cpu_ids, which CONFIG_NR_CPUS caps at 8192 on x86-64 and 4096 on arm64, so
// room for 65536 CPUs is enough for one call. If a kernel ever had more, the
// count falls back to hardware_concurrency().
static std::optional<size_t> AffinityCpuCount()
{
    constexpr size_t MAX_AFFINITY_CPUS = 65536;
    std::array<cpu_set_t, MAX_AFFINITY_CPUS / CPU_SETSIZE> mask{};
    static_assert(sizeof(mask) * CHAR_BIT == MAX_AFFINITY_CPUS);
    if(sched_getaffinity(getpid(), sizeof(mask), mask.data()) != 0)
        return std::nullopt;
    int count = 0;
    for(const cpu_set_t& set : mask)
        count += CPU_COUNT(&set);
    return count > 0 ? std::optional<size_t>(static_cast<size_t>(count)) : std::nullopt;
}
#endif

size_t bsv::internal::CpuCountFrom(std::optional<size_t> affinityCpus,
                                   size_t hardwareConcurrency,
                                   std::optional<double> cgroupLimit)
{
    // hardware_concurrency() is 0 when it cannot tell.
    return std::max<size_t>(LimitCpuCount(affinityCpus.value_or(hardwareConcurrency),
                                          cgroupLimit),
                            1);
}

size_t bsv::internal::EffectiveCpuCount(const std::filesystem::path& cgroupRoot)
{
    const size_t hardware = std::thread::hardware_concurrency();
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    return CpuCountFrom(AffinityCpuCount(), hardware, CgroupCpuLimit(cgroupRoot));
#else
    (void)cgroupRoot;
    return CpuCountFrom(std::nullopt, hardware, std::nullopt);
#endif
}
