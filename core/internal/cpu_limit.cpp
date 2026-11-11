// CPUs available to this process, for sizing the input verification pool: the
// affinity mask, lowered to the CPU bandwidth limit of the process's cgroup.
// FindCpuCgroup, FindCpuCgroupDir, ParseV2Limit and ParseV1Limit take file
// contents; CgroupCpuLimit only reads the files. The lookup takes the first
// mount whose root contains the cgroup, strips that root, and skips cgroups
// outside the namespace's view of the mount.
// The cgroup parsing and the rounding were checked against Go 1.26's runtime
// with a differential test. Known differences:
// - a limit of +Inf or NaN (a zero period; the kernel's minimum is 1 ms) or
//   above 2^31 CPUs: Go converts it to int32, which is platform-specific; BDK
//   leaves the count unchanged, as such a limit could not lower it anyway.
// - Go's fixed buffers: lines over 64 KiB and paths over PATH_MAX, which the
//   kernel does not write.
// - a mount point with stray backslashes, which Go's parser indexes out of
//   range on; the kernel writes '\' as \134.
// - once the main thread has exited, proc/self/mountinfo no longer opens; BDK
//   then reads the cgroup and mountinfo files of proc/thread-self, the calling
//   thread's (see ReadCgroupAndMounts).
// - Go reads the cgroup files once and re-reads the limit and the affinity
//   mask periodically; BDK reads them on every EffectiveCpuCount call.

#include <internal/cpu_limit.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <cfile_util.h>
#include <utilstrencodings.h>

// The affinity mask is Linux's; elsewhere it is not read. The cgroup files are
// read on every platform; off Linux they are not there, and no limit applies.
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sched.h>

// CPUs in the process's affinity mask. getpid() names the main thread (its
// thread ID is the process ID), so every caller reads the main thread's mask,
// whichever thread it runs on. Once the main thread has exited, its mask no
// longer changes: cpuset updates skip exiting tasks
// (cpuset_update_tasks_cpumask), so the count is the mask as it was then.
// The kernel fails the call (EINVAL) only for a buffer with fewer bits than
// nr_cpu_ids, which CONFIG_NR_CPUS caps at 8192 on x86-64 and 4096 on arm64, so
// room for 65536 CPUs is enough for one call. If a kernel ever had more, the
// count falls back to hardware_concurrency(). Mirrors Go's getCPUCount.
static std::optional<size_t> AffinityCpuCount()
{
    constexpr size_t MAX_AFFINITY_CPUS = 65'536;
    std::array<cpu_set_t, MAX_AFFINITY_CPUS / CPU_SETSIZE> mask{};
    static_assert(sizeof(mask) * CHAR_BIT == MAX_AFFINITY_CPUS);
    if(sched_getaffinity(getpid(), sizeof(mask), mask.data()) != 0)
        return std::nullopt;

    int count = 0;
    for(const cpu_set_t& set : mask)
        count += CPU_COUNT(&set);
    return count > 0 ? std::optional<size_t>(static_cast<size_t>(count)) : std::nullopt;
}

#else
static std::optional<size_t> AffinityCpuCount() { return std::nullopt; }
#endif

namespace
{

    struct Line
    {
        std::string_view text; // without its '\n'
        bool complete;         // false for a last line with no '\n'
    };

}

// The lines of a /proc file: nothing after a final '\n', and a last line
// without one is incomplete.
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
static std::optional<std::string_view> SingleLine(const std::string_view content)
{
    const size_t newline = content.find('\n');
    if(newline == std::string_view::npos)
        return std::nullopt;

    return content.substr(0, newline);
}

static bool ContainsCpu(const std::string_view commaSeparated)
{
    static constexpr std::string_view CPU{"cpu"};
    return std::ranges::any_of(std::views::split(commaSeparated, ','),
                               [](auto item) { return std::ranges::equal(item, CPU); });
}

// cgroup path `path` is `root` or below it. `root` starts with '/'.
static bool HasPathPrefix(const std::string_view path, const std::string_view root)
{
    if(root.size() == 1)
        return true;

    return path.starts_with(root)
        && (path.size() == root.size() || path[root.size()] == '/');
}

using bsv::internal::CgroupError;
using bsv::internal::CgroupVersion;
using bsv::internal::CpuCgroup;

// mountinfo escapes space, tab, newline and backslash as \ooo.
static std::variant<std::string, CgroupError> UnescapeMountPath(
    const std::string_view text)
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
            return CgroupError::InvalidEscape;

        int value = 0;
        for(size_t k = 1; k <= 3; ++k)
        {
            const char c = text[i + k];
            if(c < '0' || c > '7')
                return CgroupError::InvalidEscape;

            value = (value * 8) + (c - '0');
        }
        if(value > MAX_ESCAPED_BYTE)
            return CgroupError::InvalidEscape;

        out.push_back(static_cast<char>(value));
        i += 3;
    }
    return out;
}

// Lines are "hierarchy-ID:controller-list:cgroup-path". Any line up to the v1
// match that is malformed or has no '\n' rejects the file.
std::variant<CpuCgroup, CgroupError> bsv::internal::FindCpuCgroup(
    const std::string_view cgroupFile)
{
    std::optional<CpuCgroup> v2;
    for(const auto& [line, complete] : Lines(cgroupFile))
    {
        if(!complete)
            return CgroupError::IncompleteLine;

        const size_t first = line.find(':');
        const size_t second = first == std::string_view::npos ? first
                                                              : line.find(':', first + 1);
        if(second == std::string_view::npos)
            return CgroupError::MalformedFile;

        const std::string_view hierarchy = line.substr(0, first);
        const std::string_view controllers = line.substr(first + 1, second - first - 1);
        const std::string_view path = line.substr(second + 1);
        if(path.empty() || path[0] != '/')
            return CgroupError::MalformedFile;

        if(hierarchy == "0")
            v2 = CpuCgroup{.version = CgroupVersion::V2, .path = std::string(path)};
        else if(ContainsCpu(controllers))
            return CpuCgroup{.version = CgroupVersion::V1, .path = std::string(path)};
    }
    if(!v2)
        return CgroupError::NoCgroup;

    return *v2;
}

// Finds the mount and builds the directory under it. A line is
//   ID parent-ID major:minor root mount-point options [optional...] - type source
//   super-options
// Every line up to the match needs a root starting with '/', a " - "
// separator and a type followed by a space; a cgroup mount also needs its
// source. Roots are unescaped on mounts of the wanted type, and the mount
// point on the match only.
std::variant<std::string, CgroupError> bsv::internal::FindCpuCgroupDir(
    const std::string_view mountinfo,
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
            return CgroupError::MalformedFile;

        const auto root = next();
        if(!root || root->empty() || (*root)[0] != '/')
            return CgroupError::MalformedFile;

        const auto mountPoint = next();
        if(!mountPoint)
            return CgroupError::MalformedFile;

        for(;;)
        { // options and optional fields, up to " - "
            const size_t space = line.find(' ');
            if(space == std::string_view::npos || space + 3 >= line.size())
                return CgroupError::MalformedFile;

            if(line.substr(space, 3) == " - ")
            {
                line.remove_prefix(space + 3);
                break;
            }
            line.remove_prefix(space + 1);
        }
        const auto fsType = next();
        if(!fsType)
            return CgroupError::MalformedFile;

        if(cgroup.version == CgroupVersion::V1)
        {
            if(*fsType != "cgroup")
                continue;
            if(!next()) // source
                return CgroupError::MalformedFile;

            if(!ContainsCpu(line)) // super options, the rest of the line
                continue;
        }
        else if(*fsType != "cgroup2")
        {
            continue;
        }

        const auto unescapedRoot = UnescapeMountPath(*root);
        if(const auto* error = std::get_if<CgroupError>(&unescapedRoot))
            return *error;

        const auto& rootPath = std::get<std::string>(unescapedRoot);
        if(!HasPathPrefix(cgroup.path, rootPath))
            continue;
        std::string_view rel = std::string_view(cgroup.path).substr(rootPath.size());
        if(rootPath.size() == 1 && cgroup.path.size() > 1)
            rel = cgroup.path;
        if(HasPathPrefix(rel, "/.."))
            continue; // outside this cgroup namespace's view of the mount
        const auto unescapedMountPoint = UnescapeMountPath(*mountPoint);
        if(const auto* error = std::get_if<CgroupError>(&unescapedMountPoint))
            return *error;

        // Concatenated, not joined with operator/: the matching above is
        // byte-wise and lexical, and so is the result.
        return std::get<std::string>(unescapedMountPoint) + std::string(rel);
    }
    return CgroupError::NoCgroup;
}

// quota / period in CPUs, as double division gives it: a zero or negative quota
// is a limit, which the rounding raises to MIN_LIMITED_CPUS, and a zero period
// gives +Inf, -Inf or NaN (spelled out here, as dividing by zero is undefined in
// C++).
static double QuotaPerPeriod(const int64_t quota, const int64_t period)
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

// Integers are SV Node's ParseInt64: base 10, an optional '+' or '-', the
// whole string, int64 range.
std::variant<double, CgroupError> bsv::internal::ParseV2Limit(
    const std::string_view cpuMax)
{
    const size_t space = cpuMax.find(' ');
    if(space == std::string_view::npos)
        return CgroupError::MalformedFile;

    if(cpuMax.substr(0, space) == "max")
        return std::numeric_limits<double>::infinity(); // no limit

    const auto periodLine = SingleLine(cpuMax.substr(space + 1));
    if(!periodLine)
        return CgroupError::MalformedFile;

    int64_t quota{};
    int64_t period{};
    if(!ParseInt64(std::string(cpuMax.substr(0, space)), &quota)
       || !ParseInt64(std::string(*periodLine), &period))
        return CgroupError::MalformedFile;

    return QuotaPerPeriod(quota, period);
}

// cpu.cfs_quota_us or cpu.cfs_period_us, "<value>\n".
static std::variant<int64_t, CgroupError> ParseV1Number(const std::string_view content)
{
    const auto line = SingleLine(content);
    int64_t value{};
    if(!line || !ParseInt64(std::string(*line), &value))
        return CgroupError::MalformedFile;

    return value;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): the two files, by name
std::variant<double, CgroupError> bsv::internal::ParseV1Limit(
    const std::string_view quota,
    const std::string_view period)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    const auto quotaValue = ParseV1Number(quota);
    if(const auto* error = std::get_if<CgroupError>(&quotaValue))
        return *error;

    if(std::get<int64_t>(quotaValue) < 0)
        return std::numeric_limits<double>::infinity(); // negative, usually -1: no limit

    const auto periodValue = ParseV1Number(period);
    if(const auto* error = std::get_if<CgroupError>(&periodValue))
        return *error;

    return QuotaPerPeriod(std::get<int64_t>(quotaValue), std::get<int64_t>(periodValue));
}

// The descriptor, or open()'s errno.
static std::variant<UniqueFileDescriptor, int> OpenReadOnly(
    const std::filesystem::path& path)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if(fd < 0)
        return errno;

    return UniqueFileDescriptor{fd};
}

// The contents of a cgroup or mountinfo file under proc: NoCgroup when the file
// does not exist, SyscallFailed when it does not open or read. Read with open()
// and read(), not a stream, as only open()'s errno tells a missing file from one
// that does not open.
static std::variant<std::string, CgroupError> ReadProcFile(
    const std::filesystem::path& path)
{
    const auto file = OpenReadOnly(path);
    if(const int* error = std::get_if<int>(&file))
        return *error == ENOENT ? CgroupError::NoCgroup : CgroupError::SyscallFailed;

    const int fd = std::get<UniqueFileDescriptor>(file).Get();
    constexpr size_t CHUNK_SIZE = 4096;
    std::string content;
    for(;;)
    {
        const size_t size = content.size();
        content.resize(size + CHUNK_SIZE);
        const ssize_t bytes = read(fd, content.data() + size, CHUNK_SIZE);
        if(bytes < 0)
            return CgroupError::SyscallFailed;

        content.resize(size + static_cast<size_t>(bytes));
        if(bytes == 0)
            return content;
    }
}

// A cgroup value file, read from the start with one pread into a buffer of
// VALUE_FILE_SIZE_LIMIT bytes: SyscallFailed when the read fails, MalformedFile
// when the file fills the buffer.
static std::variant<std::string, CgroupError> ReadValueFile(
    const UniqueFileDescriptor& file)
{
    std::array<char, bsv::internal::VALUE_FILE_SIZE_LIMIT> buffer{};
    const ssize_t bytes = pread(file.Get(), buffer.data(), buffer.size(), 0);
    if(bytes < 0)
        return CgroupError::SyscallFailed;

    if(static_cast<size_t>(bytes) == buffer.size())
        return CgroupError::MalformedFile;

    return std::string(buffer.data(), static_cast<size_t>(bytes));
}

// The cgroup in a proc/self/cgroup file: its read error, or FindCpuCgroup's
// result.
static std::variant<CpuCgroup, CgroupError> ReadCpuCgroup(
    const std::filesystem::path& path)
{
    const auto content = ReadProcFile(path);
    if(const auto* error = std::get_if<CgroupError>(&content))
        return *error;

    return bsv::internal::FindCpuCgroup(std::get<std::string>(content));
}

// The cgroup and the mountinfo file to find it in, both from proc/self, the
// main thread's; or both from proc/thread-self, the calling thread's, when
// proc/self/mountinfo does not read. That happens once the main thread has
// exited: proc/self is then that exited thread's, and its mountinfo no longer
// opens (EINVAL, as exit_task_namespaces has cleared its nsproxy, which
// mounts_open_common requires). On a cgroup v1 hierarchy its cgroup then reads
// as "/" (proc_cgroup_show prints that for an exiting task), so the cgroup file
// comes from the same directory as mountinfo, never from the other one.
// thread-self's files are the process's unless the calling thread has its own
// mount namespace or was moved to another cgroup on its own. Kernels before
// 3.17 have no thread-self. The cgroup file is read and parsed before mountinfo
// is opened, so its errors come first; when neither mountinfo reads, the error
// is proc/self's.
static std::variant<std::pair<CpuCgroup, std::string>, CgroupError> ReadCgroupAndMounts(
    const std::filesystem::path& root)
{
    auto cgroup = ReadCpuCgroup(root / "proc/self/cgroup");
    if(const auto* error = std::get_if<CgroupError>(&cgroup))
        return *error;

    auto mountinfo = ReadProcFile(root / "proc/self/mountinfo");
    if(const auto* error = std::get_if<CgroupError>(&mountinfo))
    {
        auto threadSelf = ReadProcFile(root / "proc/thread-self/mountinfo");
        if(std::holds_alternative<CgroupError>(threadSelf))
            return *error;

        cgroup = ReadCpuCgroup(root / "proc/thread-self/cgroup");
        if(const auto* cgroupError = std::get_if<CgroupError>(&cgroup))
            return *cgroupError;

        mountinfo = std::move(threadSelf);
    }
    return std::pair{std::move(std::get<CpuCgroup>(cgroup)),
                     std::move(std::get<std::string>(mountinfo))};
}

std::variant<double, CgroupError> bsv::internal::CgroupCpuLimit(
    const std::filesystem::path& root)
{
    const auto files = ReadCgroupAndMounts(root);
    if(const auto* error = std::get_if<CgroupError>(&files))
        return *error;

    const auto& [cgroup, mountinfo] = std::get<std::pair<CpuCgroup, std::string>>(files);
    const auto dir = FindCpuCgroupDir(mountinfo, cgroup);
    if(const auto* error = std::get_if<CgroupError>(&dir))
        return *error;

    // The directory is absolute; operator/ would let it replace `root`.
    const std::filesystem::path
        cgroupDir = root
                  / std::filesystem::path(std::get<std::string>(dir)).relative_path();

    // Each value file is opened before any is read, so a v1 file that does not
    // open is SyscallFailed whatever the other one holds.
    if(cgroup.version == CgroupVersion::V2)
    {
        const auto cpuMax = OpenReadOnly(cgroupDir / "cpu.max");
        const auto* cpuMaxFile = std::get_if<UniqueFileDescriptor>(&cpuMax);
        if(!cpuMaxFile)
            return CgroupError::SyscallFailed;

        const auto content = ReadValueFile(*cpuMaxFile);
        if(const auto* error = std::get_if<CgroupError>(&content))
            return *error;

        return ParseV2Limit(std::get<std::string>(content));
    }

    const auto quota = OpenReadOnly(cgroupDir / "cpu.cfs_quota_us");
    const auto period = OpenReadOnly(cgroupDir / "cpu.cfs_period_us");
    const auto* quotaFile = std::get_if<UniqueFileDescriptor>(&quota);
    const auto* periodFile = std::get_if<UniqueFileDescriptor>(&period);
    if(!quotaFile || !periodFile)
        return CgroupError::SyscallFailed;

    // A v1 value file that does not read is passed on as empty, so ParseV1Limit
    // reports it as MalformedFile, unless it is the period next to a quota that
    // is no limit, which ParseV1Limit does not parse.
    const auto contentOrEmpty = [](const UniqueFileDescriptor& file)
    {
        auto content = ReadValueFile(file);
        auto* text = std::get_if<std::string>(&content);
        return text ? std::move(*text) : std::string();
    };
    return ParseV1Limit(contentOrEmpty(*quotaFile), contentOrEmpty(*periodFile));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): a count and a limit, by name
size_t bsv::internal::LimitCpuCount(const size_t cpus, const double cgroupLimit)
{
    // NaN, +Inf and every limit of at least `cpus` leave it unchanged, which
    // also keeps the conversion below in range.
    const double rounded = std::ceil(cgroupLimit);
    if(std::isnan(rounded) || rounded >= static_cast<double>(cpus))
        return cpus;

    // -Inf, zero and negative limits count as no CPUs, raised to the minimum.
    const size_t count = rounded > 0 ? static_cast<size_t>(rounded) : 0;
    return std::min(std::max(count, MIN_LIMITED_CPUS), cpus);
}

size_t bsv::internal::CpuCountFrom(const std::optional<size_t> affinityCpus,
                                   const size_t hardwareConcurrency,
                                   const double cgroupLimit)
{
    // hardware_concurrency() is 0 when it cannot tell.
    return std::max<size_t>(LimitCpuCount(affinityCpus.value_or(hardwareConcurrency),
                                          cgroupLimit),
                            1);
}

size_t bsv::internal::EffectiveCpuCount(const std::filesystem::path& cgroupRoot)
{
    const auto limit = CgroupCpuLimit(cgroupRoot);
    const auto* value = std::get_if<double>(&limit);
    return CpuCountFrom(AffinityCpuCount(),
                        std::thread::hardware_concurrency(),
                        value ? *value : std::numeric_limits<double>::infinity());
}
