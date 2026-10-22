// CPUs available to this process, for sizing the input verification pool.
// The cgroup lookup follows Go 1.26's internal/runtime/cgroup (FindCPUCgroup,
// FindCPUMountPoint: match the mount whose root contains the cgroup, strip
// that root, skip paths outside the cgroup namespace, take the first match).
// Go 1.25 ignored the mount root and kept the last cgroup2 mount. The rounding
// is Go's default GOMAXPROCS rule, unchanged since go1.25.0. With GOMAXPROCS
// unset and Go 1.26 or later, BDK and the Go runtime in one process derive the
// same CPU count, except that Go re-reads the limit and BDK reads it once.

#include <internal/txvalidator_internal.hpp>

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sched.h>
#endif

namespace {

std::optional<std::string> ReadFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::ostringstream content;
    content << in.rdbuf();
    if (in.bad())
        return std::nullopt;
    return content.str();
}

std::vector<std::string_view> Split(std::string_view text, char sep)
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

// Whole-string base-10 integer, no surrounding whitespace.
std::optional<int64_t> ParseInt(std::string_view text)
{
    if (text.empty())
        return std::nullopt;
    int64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// The single line of a cgroup file, without its newline.
std::optional<std::string_view> SingleLine(std::string_view content)
{
    const size_t newline = content.find('\n');
    if (newline == std::string_view::npos)
        return std::nullopt;
    return content.substr(0, newline);
}

bool ContainsCpu(std::string_view commaSeparated)
{
    for (const std::string_view item : Split(commaSeparated, ','))
        if (item == "cpu")
            return true;
    return false;
}

// cgroup path `path` is `root` or below it.
bool HasPathPrefix(std::string_view path, std::string_view root)
{
    if (root == "/")
        return true;
    return path.starts_with(root) && (path.size() == root.size() || path[root.size()] == '/');
}

// mountinfo escapes space, tab, newline and backslash as \ooo.
std::optional<std::string> UnescapeMountPath(std::string_view text)
{
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
        out.push_back(static_cast<char>(value));
        i += 3;
    }
    return out;
}

enum class CgroupVersion : uint8_t { V1, V2 };

struct CpuCgroup {
    CgroupVersion version;
    std::string path;  // relative to its hierarchy's root, starts with '/'
};

// From proc/self/cgroup: the v1 hierarchy with the cpu controller, else the
// v2 hierarchy (ID 0).
std::optional<CpuCgroup> FindCpuCgroup(std::string_view content)
{
    std::optional<CpuCgroup> v2;
    for (const std::string_view line : Split(content, '\n')) {
        if (line.empty())
            continue;
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

// From proc/self/mountinfo: the directory holding `cgroup`'s files, the first
// matching cgroup2 mount, or cgroup mount with the cpu option, whose root
// contains the cgroup.
std::optional<std::string> FindCpuCgroupDir(std::string_view content, const CpuCgroup& cgroup)
{
    for (const std::string_view line : Split(content, '\n')) {
        if (line.empty())
            continue;
        const auto fields = Split(line, ' ');
        // ID, parent ID, major:minor, root, mount point, options, optional
        // fields..., "-", filesystem type, source, super options.
        const auto separator = std::ranges::find(fields, std::string_view("-"));
        if (fields.size() < 6 || separator == fields.end() || separator - fields.begin() < 6
            || fields.end() - separator < 4)
            return std::nullopt;
        const std::string_view fsType = separator[1];
        const std::string_view superOptions = separator[3];
        const bool cpuMount = cgroup.version == CgroupVersion::V2
            ? fsType == "cgroup2"
            : fsType == "cgroup" && ContainsCpu(superOptions);
        if (!cpuMount)
            continue;

        const auto root = UnescapeMountPath(fields[3]);
        const auto mountPoint = UnescapeMountPath(fields[4]);
        if (!root || !mountPoint || root->empty() || (*root)[0] != '/')
            return std::nullopt;
        if (!HasPathPrefix(cgroup.path, *root))
            continue;
        const std::string_view rel = *root == "/" ? std::string_view(cgroup.path)
                                                  : std::string_view(cgroup.path).substr(root->size());
        if (HasPathPrefix(rel, "/.."))
            continue;  // outside this cgroup namespace's view of the mount
        return *mountPoint + std::string(rel == "/" ? std::string_view() : rel);
    }
    return std::nullopt;
}

// cpu.max: "<quota> <period>\n", quota "max" for no limit.
std::optional<double> ParseV2Limit(std::string_view content)
{
    const auto line = SingleLine(content);
    if (!line)
        return std::nullopt;
    const size_t space = line->find(' ');
    if (space == std::string_view::npos)
        return std::nullopt;
    if (line->substr(0, space) == "max")
        return std::nullopt;
    const auto quota = ParseInt(line->substr(0, space));
    const auto period = ParseInt(line->substr(space + 1));
    if (!quota || !period || *quota <= 0 || *period <= 0)
        return std::nullopt;
    return static_cast<double>(*quota) / static_cast<double>(*period);
}

} // namespace

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
    const auto quotaLine = quotaFile ? SingleLine(*quotaFile) : std::nullopt;
    const auto quota = quotaLine ? ParseInt(*quotaLine) : std::nullopt;
    if (!quota || *quota < 0)
        return std::nullopt;  // -1: no limit
    const auto periodFile = ReadFile(base + *dir + "/cpu.cfs_period_us");
    const auto periodLine = periodFile ? SingleLine(*periodFile) : std::nullopt;
    const auto period = periodLine ? ParseInt(*periodLine) : std::nullopt;
    if (!period || *period <= 0 || *quota == 0)
        return std::nullopt;
    return static_cast<double>(*quota) / static_cast<double>(*period);
}

size_t bsv::detail::LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit)
{
    if (!cgroupLimit)
        return cpus;
    const double limit = std::max(std::ceil(*cgroupLimit), 2.0);
    return limit < static_cast<double>(cpus) ? static_cast<size_t>(limit) : cpus;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
// CPUs in the calling thread's affinity mask, sized for any CPU count.
static std::optional<size_t> AffinityCpuCount()
{
    constexpr int INITIAL_AFFINITY_CPUS = 1024;
    // Bound probing to a 128 KiB mask even if the kernel rejects its size.
    constexpr int MAX_AFFINITY_CPUS = 1 << 20;
    const auto freeMask = [](cpu_set_t* mask) { CPU_FREE(mask); };
    for (int ncpus = INITIAL_AFFINITY_CPUS; ncpus <= MAX_AFFINITY_CPUS; ncpus *= 2) {
        std::unique_ptr<cpu_set_t, decltype(freeMask)> mask{ CPU_ALLOC(ncpus), freeMask };
        if (mask == nullptr)
            return std::nullopt;
        const size_t size = CPU_ALLOC_SIZE(ncpus);
        CPU_ZERO_S(size, mask.get());
        const int rc = sched_getaffinity(0, size, mask.get());
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
    size_t cpus = std::thread::hardware_concurrency();
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (const auto allowed = AffinityCpuCount())
        cpus = cpus == 0 ? *allowed : std::min(cpus, *allowed);
    cpus = LimitCpuCount(cpus, CgroupCpuLimit(cgroupRoot));
#else
    (void)cgroupRoot;
#endif
    return cpus;
}
