#include <detail/cgroup.hpp>

#include <detail/file_io.hpp>
#include <detail/mountinfo.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <utilstrencodings.h>

using namespace bsv::detail;

std::ostream& bsv::detail::operator<<(std::ostream& os, const CgroupError error)
{
    switch(error)
    {
    case CgroupError::NoCgroup:
        return os << "NoCgroup";
    case CgroupError::MalformedFile:
        return os << "MalformedFile";
    case CgroupError::InvalidEscape:
        return os << "InvalidEscape";
    case CgroupError::IncompleteLine:
        return os << "IncompleteLine";
    case CgroupError::SyscallFailed:
        return os << "SyscallFailed";
    }
    return os << "CgroupError " << static_cast<int>(error);
}

// Only open()'s errno tells a missing file from one that does not open,
// which is why the files are read with file_io, not a stream.
static CgroupError ToCgroupError(const std::errc error)
{
    switch(error)
    {
    case std::errc::no_such_file_or_directory:
        return CgroupError::NoCgroup;
    case std::errc::file_too_large:
        return CgroupError::MalformedFile;
    default:
        return CgroupError::SyscallFailed;
    }
}

std::ostream& bsv::detail::operator<<(std::ostream& os, const CgroupDir& dir)
{
    return os << (dir.version == CgroupVersion::V1 ? "v1 " : "v2 ") << dir.path;
}

namespace
{
    // The contents of one proc directory's files, self's or thread-self's.
    struct ProcFiles
    {
        std::string cgroup;
        std::string mountinfo;
    };
}

static tl::expected<ProcFiles, std::errc> ReadProcFiles(const std::filesystem::path& dir)
{
    return ReadFile(dir / "cgroup")
        .and_then(
            [&dir](const std::string& cgroup)
            {
                return ReadFile(dir / "mountinfo")
                    .transform(
                        [&cgroup](const std::string& mountinfo)
                        { return ProcFiles{.cgroup = cgroup, .mountinfo = mountinfo}; });
            });
}

static tl::expected<CgroupDir, CgroupError> ParseProcFiles(const ProcFiles& files)
{
    return ParseCpuCgroup(files.cgroup)
        .and_then([&files](const CpuCgroup& cgroup)
                  { return ParseCpuCgroupDir(files.mountinfo, cgroup); });
}

// Once the main thread has exited, proc/self is that exited thread's, and
// its mountinfo no longer opens (EINVAL, as exit_task_namespaces has
// cleared its nsproxy, which mounts_open_common requires). On a cgroup v1
// hierarchy its cgroup then reads as "/" (proc_cgroup_show prints that for
// an exiting task), so the cgroup file must come from the same directory as
// mountinfo. thread-self's files are the process's unless the calling
// thread has its own mount namespace or was moved to another cgroup on its
// own. Kernels before 3.17 have no thread-self.
tl::expected<CgroupDir, CgroupError> bsv::detail::FindCgroupDir(
    const std::filesystem::path& root)
{
    return ReadProcFiles(root / "proc/self")
        .or_else(
            [&root](const std::errc selfError)
            {
                return ReadProcFiles(root / "proc/thread-self")
                    .or_else([selfError](const std::errc /*threadSelfError*/)
                                 -> tl::expected<ProcFiles, std::errc>
                             { return tl::unexpected{selfError}; });
            })
        .transform_error(ToCgroupError)
        .and_then(ParseProcFiles)
        // The directory is absolute; operator/ would let it replace `root`.
        .transform(
            [&root](const CgroupDir& dir)
            {
                return CgroupDir{.version = dir.version,
                                 .path = root / dir.path.relative_path()};
            });
}

bsv::detail::CpuBandwidth::CpuBandwidth(const CpuQuota quota, const int64_t period):
    quota_{quota},
    period_{period}
{
}

tl::expected<CpuBandwidth, CgroupError> bsv::detail::CpuBandwidth::Make(
    const CpuQuota quota,
    const int64_t period)
{
    const bool negative{std::holds_alternative<int64_t>(quota)
                        && std::get<int64_t>(quota) < 0};
    if(negative || period <= 0)
        return tl::unexpected{CgroupError::MalformedFile};

    return CpuBandwidth{quota, period};
}

uint64_t bsv::detail::CpuBandwidth::CpusNeeded() const
{
    const auto quota{std::get<int64_t>(quota_)};
    return static_cast<uint64_t>((quota / period_) + (quota % period_ > 0 ? 1 : 0));
}

std::ostream& bsv::detail::operator<<(std::ostream& os, const CpuBandwidth& bandwidth)
{
    if(bandwidth.IsUnlimited())
        os << "max";
    else
        os << std::get<int64_t>(bandwidth.Quota());
    return os << ' ' << bandwidth.Period();
}

// Cgroup value files (cpu.max, cpu.cfs_quota_us, cpu.cfs_period_us) hold one
// or two integers and a newline. A file of this many bytes or more is not a
// value file: ReadCpuBandwidth reads no further and reports MalformedFile.
constexpr size_t VALUE_FILE_SIZE_LIMIT = 64;

static tl::expected<std::string, CgroupError> ReadLimitFile(
    const std::filesystem::path& path)
{
    return ReadSmallFile(path, VALUE_FILE_SIZE_LIMIT).transform_error(ToCgroupError);
}

static tl::expected<CpuBandwidth, CgroupError> ReadCfsBandwidth(
    const std::filesystem::path& dir)
{
    return ReadLimitFile(dir / "cpu.cfs_quota_us")
        .and_then(
            [&dir](const std::string& quota)
            {
                return ReadLimitFile(dir / "cpu.cfs_period_us")
                    .and_then([&quota](const std::string& period)
                              { return ParseCfsBandwidth(quota, period); });
            });
}

tl::expected<CpuBandwidth, CgroupError> bsv::detail::ReadCpuBandwidth(
    const CgroupDir& dir)
{
    if(dir.version == CgroupVersion::V2)
        return ReadLimitFile(dir.path / "cpu.max").and_then(ParseCpuMax);

    return ReadCfsBandwidth(dir.path);
}

bsv::detail::CpuCgroup::CpuCgroup(const CgroupVersion version, std::string path):
    version_{version},
    path_{std::move(path)}
{
}

tl::expected<CpuCgroup, CgroupError> bsv::detail::CpuCgroup::Make(
    const CgroupVersion version,
    std::string path)
{
    if(!path.starts_with('/'))
        return tl::unexpected{CgroupError::MalformedFile};

    return CpuCgroup{version, std::move(path)};
}

std::ostream& bsv::detail::operator<<(std::ostream& os, const CpuCgroup& cgroup)
{
    return os << (cgroup.Version() == CgroupVersion::V1 ? "v1 " : "v2 ") << cgroup.Path();
}

namespace
{
    struct Line
    {
        std::string_view text; // without its '\n'
        bool terminated;       // false for a last line with no '\n'
    };
}

static std::vector<Line> Lines(std::string_view content)
{
    std::vector<Line> lines;
    while(!content.empty())
    {
        const size_t newline = content.find('\n');
        if(newline == std::string_view::npos)
        {
            lines.push_back(Line{.text = content, .terminated = false});
            break;
        }
        lines.push_back(Line{.text = content.substr(0, newline), .terminated = true});
        content.remove_prefix(newline + 1);
    }
    return lines;
}

static bool ContainsCpu(const std::string_view commaSeparated)
{
    static constexpr std::string_view CPU{"cpu"};
    return std::ranges::any_of(std::views::split(commaSeparated, ','),
                               [](auto item) { return std::ranges::equal(item, CPU); });
}

// Lines are "hierarchy-ID:controller-list:cgroup-path".
tl::expected<CpuCgroup, CgroupError> bsv::detail::ParseCpuCgroup(
    const std::string_view cgroupFile)
{
    std::optional<CpuCgroup> v2;
    for(const auto& [line, terminated] : Lines(cgroupFile))
    {
        if(!terminated)
            return tl::unexpected{CgroupError::IncompleteLine};

        const size_t first = line.find(':');
        const size_t second = first == std::string_view::npos ? first
                                                              : line.find(':', first + 1);
        if(second == std::string_view::npos)
            return tl::unexpected{CgroupError::MalformedFile};

        const std::string_view hierarchy = line.substr(0, first);
        const std::string_view controllers = line.substr(first + 1, second - first - 1);
        const auto cgroup{
            CpuCgroup::Make(hierarchy == "0" ? CgroupVersion::V2 : CgroupVersion::V1,
                            std::string(line.substr(second + 1)))};
        if(!cgroup)
            return cgroup;

        if(hierarchy == "0")
            v2 = *cgroup;
        else if(ContainsCpu(controllers))
            return cgroup;
    }
    if(!v2)
        return tl::unexpected{CgroupError::NoCgroup};

    return *v2;
}

// `root` starts with '/'.
static bool HasPathPrefix(const std::string_view path, const std::string_view root)
{
    if(root.size() == 1)
        return true;

    return path.starts_with(root)
        && (path.size() == root.size() || path[root.size()] == '/');
}

static CgroupError FromMountinfoError(const MountinfoError error)
{
    switch(error)
    {
    case MountinfoError::MalformedLine:
        return CgroupError::MalformedFile;
    case MountinfoError::InvalidEscape:
        return CgroupError::InvalidEscape;
    }
    return CgroupError::MalformedFile;
}

// A cgroup2 mount for v2; for v1, a cgroup mount with the cpu controller in its
// super options. MalformedFile for a cgroup mount without its source.
static tl::expected<bool, CgroupError> IsCpuMount(const MountFields& fields,
                                                  const CgroupVersion version)
{
    if(version == CgroupVersion::V2)
        return fields.fsType == "cgroup2";
    if(fields.fsType != "cgroup")
        return false;

    const size_t space = fields.rest.find(' '); // after the source
    if(space == std::string_view::npos)
        return tl::unexpected{CgroupError::MalformedFile};

    return ContainsCpu(fields.rest.substr(space + 1));
}

// `cgroupPath` relative to the mount's `root`, or nullopt when the mount does
// not hold it. Both start with '/'.
static std::optional<std::string_view> PathBelowRoot(const std::string_view cgroupPath,
                                                     const std::string_view root)
{
    if(!HasPathPrefix(cgroupPath, root))
        return std::nullopt;

    std::string_view rel{cgroupPath.substr(root.size())};
    if(root.size() == 1 && cgroupPath.size() > 1)
        rel = cgroupPath;
    if(HasPathPrefix(rel, "/.."))
        return std::nullopt; // outside this cgroup namespace's view of the mount

    return rel;
}

// Roots are unescaped on mounts of the wanted type, and the mount point on the
// match only.
tl::expected<CgroupDir, CgroupError> bsv::detail::ParseCpuCgroupDir(
    const std::string_view mountinfo,
    const CpuCgroup& cgroup)
{
    for(const auto& entry : Lines(mountinfo))
    {
        const auto fields = SplitMountLine(entry.text)
                                .transform_error(FromMountinfoError);
        if(!fields)
            return tl::unexpected{fields.error()};

        const auto cpuMount = IsCpuMount(*fields, cgroup.Version());
        if(!cpuMount)
            return tl::unexpected{cpuMount.error()};
        if(!*cpuMount)
            continue;

        const auto rootPath = UnescapeMountPath(fields->root)
                                  .transform_error(FromMountinfoError);
        if(!rootPath)
            return tl::unexpected{rootPath.error()};

        const auto rel = PathBelowRoot(cgroup.Path(), *rootPath);
        if(!rel)
            continue;

        // Concatenated, not joined with operator/: the matching above is
        // byte-wise and lexical, and so is the result.
        return UnescapeMountPath(fields->mountPoint)
            .transform_error(FromMountinfoError)
            .transform(
                [&cgroup, rel = *rel](const std::string& mountPath)
                {
                    return CgroupDir{.version = cgroup.Version(),
                                     .path = mountPath + std::string(rel)};
                });
    }
    return tl::unexpected{CgroupError::NoCgroup};
}

static tl::expected<std::string_view, CgroupError> SingleLine(
    const std::string_view content)
{
    const size_t newline = content.find('\n');
    if(newline == std::string_view::npos)
        return tl::unexpected{CgroupError::MalformedFile};

    return content.substr(0, newline);
}

static tl::expected<int64_t, CgroupError> ParseInteger(const std::string_view text)
{
    int64_t value{};
    if(!ParseInt64(std::string(text), &value))
        return tl::unexpected{CgroupError::MalformedFile};

    return value;
}

static tl::expected<CpuQuota, CgroupError> ParseCpuMaxQuota(const std::string_view field)
{
    if(field == "max")
        return Unlimited{};

    return ParseInteger(field).transform([](const int64_t quota)
                                         { return CpuQuota{quota}; });
}

static tl::expected<CpuBandwidth, CgroupError> ParseCpuMaxLine(
    const std::string_view line)
{
    const size_t space = line.find(' ');
    if(space == std::string_view::npos)
        return tl::unexpected{CgroupError::MalformedFile};

    return ParseCpuMaxQuota(line.substr(0, space))
        .and_then(
            [period = line.substr(space + 1)](const CpuQuota quotaValue)
            {
                return ParseInteger(period).and_then(
                    [quotaValue](const int64_t periodValue)
                    { return CpuBandwidth::Make(quotaValue, periodValue); });
            });
}

tl::expected<CpuBandwidth, CgroupError> bsv::detail::ParseCpuMax(
    const std::string_view cpuMax)
{
    return SingleLine(cpuMax).and_then(ParseCpuMaxLine);
}

// The kernel writes -1 to cpu.cfs_quota_us for no limit.
static CpuQuota UnlimitedIfNegative(const int64_t quota)
{
    if(quota < 0)
        return Unlimited{};

    return quota;
}

static tl::expected<CpuQuota, CgroupError> ParseCfsQuota(const std::string_view content)
{
    return SingleLine(content).and_then(ParseInteger).transform(UnlimitedIfNegative);
}

static tl::expected<int64_t, CgroupError> ParseCfsPeriod(const std::string_view content)
{
    return SingleLine(content).and_then(ParseInteger);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): the two files, by name
tl::expected<CpuBandwidth, CgroupError> bsv::detail::ParseCfsBandwidth(
    const std::string_view quota,
    const std::string_view period)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    return ParseCfsQuota(quota).and_then(
        [period](const CpuQuota quotaValue)
        {
            return ParseCfsPeriod(period).and_then(
                [quotaValue](const int64_t periodValue)
                { return CpuBandwidth::Make(quotaValue, periodValue); });
        });
}
