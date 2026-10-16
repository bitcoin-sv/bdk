// CPUs available to this process, for sizing the input verification pool.
// The cgroup lookup follows Go's internal/runtime/cgroup (Go 1.25) so BDK and
// the Go runtime in the same process see the same limit.

#include <internal/txvalidator_internal.hpp>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
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
    const std::string s(text);
    char* end = nullptr;
    errno = 0;
    const long long value = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end != s.c_str() + s.size() || !(std::isdigit(static_cast<unsigned char>(s[0])) || s[0] == '-'))
        return std::nullopt;
    return static_cast<int64_t>(value);
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
    return path.substr(0, root.size()) == root && (path.size() == root.size() || path[root.size()] == '/');
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

enum class CgroupVersion { V1, V2 };

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
            v2 = CpuCgroup{ CgroupVersion::V2, std::string(path) };
        else if (ContainsCpu(controllers))
            return CpuCgroup{ CgroupVersion::V1, std::string(path) };
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
        const auto separator = std::find(fields.begin(), fields.end(), std::string_view("-"));
        if (fields.size() < 6 || separator == fields.end() || separator - fields.begin() < 6 || fields.end() - separator < 4)
            return std::nullopt;
        const std::string_view fsType = separator[1];
        const std::string_view superOptions = separator[3];
        if (cgroup.version == CgroupVersion::V2 ? fsType != "cgroup2" : (fsType != "cgroup" || !ContainsCpu(superOptions)))
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

size_t bsv::detail::EffectiveCpuCount()
{
    size_t cpus = std::thread::hardware_concurrency();
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
        const int allowed = CPU_COUNT(&mask);
        if (allowed > 0)
            cpus = cpus == 0 ? static_cast<size_t>(allowed) : std::min(cpus, static_cast<size_t>(allowed));
    }
    cpus = LimitCpuCount(cpus, CgroupCpuLimit("/"));
#endif
    return cpus;
}
