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
// Known differences:
// - a limit of +Inf or NaN (a zero period; the kernel's minimum is 1 ms) or
//   above 2^31 CPUs: Go converts it to int32, which is platform-specific; BDK
//   leaves the count unchanged, as such a limit could not lower it anyway.
// - Go's fixed buffers: lines over 64 KiB and paths over PATH_MAX, which the
//   kernel does not write.
// - a mount point with stray backslashes, which Go's parser indexes out of
//   range on; the kernel writes '\' as \134.
// Go also re-reads the limit and the affinity mask periodically; BDK reads
// them on every EffectiveCpuCount call.

#include <internal/cpu_limit.hpp>

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <memory>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sched.h>
#include <unistd.h>
#endif

static std::optional<std::string> ReadFile(const std::string& path)
{
    const std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::ostringstream content;
    content << in.rdbuf();
    if (in.bad())
        return std::nullopt;
    return content.str();
}

static std::vector<std::string_view> Split(std::string_view text, char sep)
{
    std::vector<std::string_view> parts;
    size_t start = 0;
    for (;;) {
        const size_t end = text.find(sep, start);
        parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            return parts;
        start = end + 1;
    }
}

namespace {

struct Line {
    std::string_view text;  // without its '\n'
    bool complete;          // false for a last line with no '\n'
};

} // namespace

// The lines of a /proc file as Go's lineReader (line_reader.go) returns them:
// nothing after a final '\n', and a last line without one is incomplete.
static std::vector<Line> Lines(std::string_view content)
{
    std::vector<Line> lines;
    while (!content.empty()) {
        const size_t newline = content.find('\n');
        if (newline == std::string_view::npos) {
            lines.push_back(Line{ .text = content, .complete = false });
            break;
        }
        lines.push_back(Line{ .text = content.substr(0, newline), .complete = true });
        content.remove_prefix(newline + 1);
    }
    return lines;
}

std::optional<int64_t> bsv::detail::ParseCgroupInt(std::string_view text)
{
    // strconv.ParseInt takes a leading '+' as well as '-'.
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-'))
            return std::nullopt;
    }
    if (text.empty())
        return std::nullopt;
    int64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// The single line of a cgroup file, without its newline.
static std::optional<std::string_view> SingleLine(std::string_view content)
{
    const size_t newline = content.find('\n');
    if (newline == std::string_view::npos)
        return std::nullopt;
    return content.substr(0, newline);
}

static bool ContainsCpu(std::string_view commaSeparated)
{
    for (const std::string_view item : Split(commaSeparated, ','))
        if (item == "cpu")
            return true;
    return false;
}

// cgroup path `path` is `root` or below it. `root` starts with '/'.
static bool HasPathPrefix(std::string_view path, std::string_view root)
{
    if (root.size() == 1)
        return true;
    return path.starts_with(root) && (path.size() == root.size() || path[root.size()] == '/');
}

// mountinfo escapes space, tab, newline and backslash as \ooo.
static std::optional<std::string> UnescapeMountPath(std::string_view text)
{
    constexpr int MAX_ESCAPED_BYTE = 0xFF;
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\') {
            out.push_back(text[i]);
            continue;
        }
        if (i + 3 >= text.size())
            return std::nullopt;
        int value = 0;
        for (size_t k = 1; k <= 3; ++k) {
            const char c = text[i + k];
            if (c < '0' || c > '7')
                return std::nullopt;
            value = value * 8 + (c - '0');
        }
        if (value > MAX_ESCAPED_BYTE)
            return std::nullopt;
        out.push_back(static_cast<char>(value));
        i += 3;
    }
    return out;
}

namespace {

enum class CgroupVersion : uint8_t { V1, V2 };

struct CpuCgroup {
    CgroupVersion version;
    std::string path;  // relative to its hierarchy's root, starts with '/'
};

} // namespace

// From proc/self/cgroup, lines "hierarchy-ID:controller-list:cgroup-path": the
// v1 hierarchy with the cpu controller, else the v2 hierarchy (ID 0). Any
// line up to the v1 match that is malformed or has no '\n' rejects the file.
static std::optional<CpuCgroup> FindCpuCgroup(std::string_view content)
{
    std::optional<CpuCgroup> v2;
    for (const auto& [line, complete] : Lines(content)) {
        if (!complete)
            return std::nullopt;
        const size_t first = line.find(':');
        const size_t second = first == std::string_view::npos ? first : line.find(':', first + 1);
        if (second == std::string_view::npos)
            return std::nullopt;
        const std::string_view hierarchy = line.substr(0, first);
        const std::string_view controllers = line.substr(first + 1, second - first - 1);
        const std::string_view path = line.substr(second + 1);
        if (path.empty() || path[0] != '/')
            return std::nullopt;
        if (hierarchy == "0")
            v2 = CpuCgroup{ .version = CgroupVersion::V2, .path = std::string(path) };
        else if (ContainsCpu(controllers))
            return CpuCgroup{ .version = CgroupVersion::V1, .path = std::string(path) };
    }
    return v2;
}

// From proc/self/mountinfo: the directory holding `cgroup`'s files, under the
// first cgroup2 mount, or cgroup mount with the cpu super option, whose root
// contains the cgroup. A line is
//   ID parent-ID major:minor root mount-point options [optional...] - type source super-options
// Every line up to the match needs a root starting with '/', a " - "
// separator and a type followed by a space; a cgroup mount also needs its
// source. Roots are unescaped on mounts of the wanted type, and the mount
// point on the match only.
static std::optional<std::string> FindCpuCgroupDir(std::string_view content, const CpuCgroup& cgroup)
{
    for (const auto& entry : Lines(content)) {
        std::string_view line = entry.text;
        // The field before the next space, which the line then starts after.
        const auto next = [&line]() -> std::optional<std::string_view> {
            const size_t space = line.find(' ');
            if (space == std::string_view::npos)
                return std::nullopt;
            const std::string_view field = line.substr(0, space);
            line.remove_prefix(space + 1);
            return field;
        };
        if (!next() || !next() || !next())  // ID, parent ID, major:minor
            return std::nullopt;
        const auto root = next();
        if (!root || root->empty() || (*root)[0] != '/')
            return std::nullopt;
        const auto mountPoint = next();
        if (!mountPoint)
            return std::nullopt;
        for (;;) {  // options and optional fields, up to " - "
            const size_t space = line.find(' ');
            if (space == std::string_view::npos || space + 3 >= line.size())
                return std::nullopt;
            if (line.substr(space, 3) == " - ") {
                line.remove_prefix(space + 3);
                break;
            }
            line.remove_prefix(space + 1);
        }
        const auto fsType = next();
        if (!fsType)
            return std::nullopt;
        if (cgroup.version == CgroupVersion::V1) {
            if (*fsType != "cgroup")
                continue;
            if (!next())  // source
                return std::nullopt;
            if (!ContainsCpu(line))  // super options, the rest of the line
                continue;
        } else if (*fsType != "cgroup2") {
            continue;
        }

        const auto unescapedRoot = UnescapeMountPath(*root);
        if (!unescapedRoot)
            return std::nullopt;
        if (!HasPathPrefix(cgroup.path, *unescapedRoot))
            continue;
        std::string_view rel = std::string_view(cgroup.path).substr(unescapedRoot->size());
        if (unescapedRoot->size() == 1 && cgroup.path.size() > 1)
            rel = cgroup.path;
        if (HasPathPrefix(rel, "/.."))
            continue;  // outside this cgroup namespace's view of the mount
        const auto unescapedMountPoint = UnescapeMountPath(*mountPoint);
        if (!unescapedMountPoint)
            return std::nullopt;
        return *unescapedMountPoint + std::string(rel);
    }
    return std::nullopt;
}

// Go reads a cgroup value file with one 64-byte pread and rejects it if that
// fills the buffer.
static constexpr size_t MAX_VALUE_FILE_SIZE = 63;

// quota / period in CPUs, as Go's float64 division gives it: a zero or
// negative quota is a limit, which the rounding raises to 2, and a zero period
// gives +Inf, -Inf or NaN (spelled out here, as dividing by zero is undefined
// in C++).
static double QuotaPerPeriod(int64_t quota, int64_t period)
{
    if (period == 0) {
        if (quota == 0)
            return std::numeric_limits<double>::quiet_NaN();
        return quota > 0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(quota) / static_cast<double>(period);
}

// cpu.max: "<quota> <period>\n", quota "max" for no limit.
static std::optional<double> ParseV2Limit(std::string_view content)
{
    if (content.size() > MAX_VALUE_FILE_SIZE)
        return std::nullopt;
    const size_t space = content.find(' ');
    if (space == std::string_view::npos)
        return std::nullopt;
    if (content.substr(0, space) == "max")
        return std::nullopt;
    const auto periodLine = SingleLine(content.substr(space + 1));
    if (!periodLine)
        return std::nullopt;
    const auto quota = bsv::detail::ParseCgroupInt(content.substr(0, space));
    const auto period = bsv::detail::ParseCgroupInt(*periodLine);
    if (!quota || !period)
        return std::nullopt;
    return QuotaPerPeriod(*quota, *period);
}

// cpu.cfs_quota_us or cpu.cfs_period_us: "<value>\n".
static std::optional<int64_t> ParseV1Number(std::string_view content)
{
    if (content.size() > MAX_VALUE_FILE_SIZE)
        return std::nullopt;
    const auto line = SingleLine(content);
    return line ? bsv::detail::ParseCgroupInt(*line) : std::nullopt;
}

std::optional<double> bsv::detail::CgroupCpuLimit(const std::string& root)
{
    const std::string base = root == "/" ? std::string() : root;
    const auto cgroupFile = ReadFile(base + "/proc/self/cgroup");
    if (!cgroupFile)
        return std::nullopt;
    const auto cgroup = FindCpuCgroup(*cgroupFile);
    if (!cgroup)
        return std::nullopt;
    const auto mountinfo = ReadFile(base + "/proc/self/mountinfo");
    if (!mountinfo)
        return std::nullopt;
    const auto dir = FindCpuCgroupDir(*mountinfo, *cgroup);
    if (!dir)
        return std::nullopt;

    if (cgroup->version == CgroupVersion::V2) {
        const auto cpuMax = ReadFile(base + *dir + "/cpu.max");
        return cpuMax ? ParseV2Limit(*cpuMax) : std::nullopt;
    }
    const auto quotaFile = ReadFile(base + *dir + "/cpu.cfs_quota_us");
    const auto quota = quotaFile ? ParseV1Number(*quotaFile) : std::nullopt;
    if (!quota || *quota < 0)
        return std::nullopt;  // negative, usually -1: no limit
    const auto periodFile = ReadFile(base + *dir + "/cpu.cfs_period_us");
    const auto period = periodFile ? ParseV1Number(*periodFile) : std::nullopt;
    if (!period)
        return std::nullopt;
    return QuotaPerPeriod(*quota, *period);
}

size_t bsv::detail::LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit)
{
    if (!cgroupLimit)
        return cpus;
    const double limit = std::max(std::ceil(*cgroupLimit), 2.0);
    return limit < static_cast<double>(cpus) ? static_cast<size_t>(limit) : cpus;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
// CPUs in the process's affinity mask, sized for any CPU count. getpid()
// names the main thread (its thread ID is the process ID), so every caller
// reads the main thread's mask, whichever thread it runs on. Go reads the
// same mask: runtime/os_linux.go getCPUCount calls sched_getaffinity(0) from
// the main thread at startup (osinit). If the main thread has exited, this
// fails with ESRCH and the count falls back to hardware_concurrency().
static std::optional<size_t> AffinityCpuCount()
{
    constexpr int INITIAL_AFFINITY_CPUS = 1024;
    // Bound probing to a 128 KiB mask even if the kernel rejects its size.
    constexpr int MAX_AFFINITY_CPUS = 1 << 20;
    const auto freeMask = [](cpu_set_t* mask) { CPU_FREE(mask); };
    for (int ncpus = INITIAL_AFFINITY_CPUS; ncpus <= MAX_AFFINITY_CPUS; ncpus *= 2) {
        const std::unique_ptr<cpu_set_t, decltype(freeMask)> mask{ CPU_ALLOC(ncpus), freeMask };
        if (mask == nullptr)
            return std::nullopt;
        const size_t size = CPU_ALLOC_SIZE(ncpus);
        CPU_ZERO_S(size, mask.get());
        const int rc = sched_getaffinity(getpid(), size, mask.get());
        const int saved = errno;
        const int count = rc == 0 ? CPU_COUNT_S(size, mask.get()) : 0;
        if (rc == 0)
            return count > 0 ? std::optional<size_t>(static_cast<size_t>(count)) : std::nullopt;
        if (saved != EINVAL)
            return std::nullopt;  // EINVAL: the mask is too small, try a larger one
    }
    return std::nullopt;
}
#endif

size_t bsv::detail::EffectiveCpuCount(const std::string& cgroupRoot)
{
    const size_t hardware = std::thread::hardware_concurrency();
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    const size_t allowed = AffinityCpuCount().value_or(hardware);
    const size_t cpus = hardware == 0 ? allowed : std::min(hardware, allowed);
    return LimitCpuCount(cpus, CgroupCpuLimit(cgroupRoot));
#else
    (void)cgroupRoot;
    return hardware;
#endif
}
