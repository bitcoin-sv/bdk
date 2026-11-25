#pragma once

// Locating this process's cgroup and reading its CPU limit.

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include <variant>

namespace bsv::detail
{
    // Why a cgroup file was rejected or could not be read.
    enum class CgroupError : uint8_t
    {
        NoCgroup,       // no cpu cgroup listed, or no mount holding it; also a
                        // file that doesn't exist
        MalformedFile,  // a file doesn't parse, or holds a negative quota or a
                        // period of zero or less; also a value file of 64 bytes
                        // or more
        InvalidEscape,  // a bad \ooo escape in a mountinfo path
        IncompleteLine, // a proc/self/cgroup line without its '\n', with no cgroup v1
                        // cpu line before it
        SyscallFailed,  // a file that exists but doesn't open or read
    };
    std::ostream& operator<<(std::ostream&, CgroupError);

    enum class CgroupVersion : uint8_t
    {
        V1,
        V2
    };

    // `version` says which limit files `path` holds.
    struct CgroupDir
    {
        CgroupVersion version;
        std::filesystem::path path;

        bool operator==(const CgroupDir&) const = default;
    };

    std::ostream& operator<<(std::ostream&, const CgroupDir&);

    // The directory holding the CPU limit files of this process's cgroup, with
    // its cgroup version; once the main thread has exited, the calling thread's
    // cgroup. `root` is where the proc and cgroup files are found ("/" on a live
    // system). NoCgroup when no cpu cgroup is found, SyscallFailed when a proc
    // file can't be read, and the parsers' errors for a malformed one.
    tl::expected<CgroupDir, CgroupError> FindCgroupDir(const std::filesystem::path& root);

    // A CPU quota with no limit: cgroup v2's "max", v1's -1.
    struct Unlimited
    {
        bool operator==(const Unlimited&) const = default;
    };

    // Microseconds of CPU time, or Unlimited.
    using CpuQuota = std::variant<int64_t, Unlimited>;

    // Up to Quota() of CPU time in every Period() microseconds, as a cgroup's
    // files hold them. The quota is not negative and the period is above zero.
    class CpuBandwidth
    {
    public:
        // MalformedFile for a negative quota, which the kernel does not write,
        // or a period of zero or less; the kernel's minimum is 1 ms.
        static tl::expected<CpuBandwidth, CgroupError> Make(CpuQuota, int64_t period);

        [[nodiscard]] const CpuQuota& Quota() const { return quota_; }

        [[nodiscard]] int64_t Period() const { return period_; }

        [[nodiscard]] bool IsUnlimited() const
        {
            return std::holds_alternative<Unlimited>(quota_);
        }

        // The fewest CPUs that can use all of the bandwidth: quota / period
        // rounded up, exactly. Requires !IsUnlimited().
        [[nodiscard]] uint64_t CpusNeeded() const;

        bool operator==(const CpuBandwidth&) const = default;

    private:
        CpuBandwidth(CpuQuota, int64_t period);

        CpuQuota quota_;
        int64_t period_;
    };

    std::ostream& operator<<(std::ostream&, const CpuBandwidth&);

    // The CPU bandwidth set in `dir`: cgroup v2 cpu.max, or cgroup v1
    // cpu.cfs_quota_us and cpu.cfs_period_us. NoCgroup when a file is missing,
    // as in the root cgroup; SyscallFailed when one can't be read; MalformedFile
    // when one doesn't parse or is 64 bytes or more.
    tl::expected<CpuBandwidth, CgroupError> ReadCpuBandwidth(const CgroupDir&);

    // The steps FindCgroupDir and ReadCpuBandwidth are built from, each on a
    // file's contents.

    // The cgroup whose CPU limit applies to this process. Its path is relative
    // to its hierarchy's root, so starts with '/'.
    class CpuCgroup
    {
        CgroupVersion version_;
        std::string path_;

        CpuCgroup(CgroupVersion, std::string path);

    public:
        // MalformedFile for a path that doesn't start with '/'.
        static tl::expected<CpuCgroup, CgroupError> Make(CgroupVersion, std::string path);

        [[nodiscard]] CgroupVersion Version() const { return version_; }

        [[nodiscard]] const std::string& Path() const { return path_; }

        bool operator==(const CpuCgroup&) const = default;
    };

    std::ostream& operator<<(std::ostream&, const CpuCgroup&);

    // From the contents of proc/self/cgroup: the v1 hierarchy with the cpu
    // controller, else the v2 hierarchy (ID 0). NoCgroup when there is neither;
    // IncompleteLine for a line without its '\n', and MalformedFile for any other
    // malformed line.
    tl::expected<CpuCgroup, CgroupError> ParseCpuCgroup(std::string_view cgroupFile);

    // From the contents of proc/self/mountinfo: the directory holding `cgroup`'s
    // files, under the first cgroup2 mount, or cgroup mount with the cpu super
    // option, whose root contains the cgroup. NoCgroup when there is none;
    // InvalidEscape for a bad \ooo escape in a mount path, and MalformedFile
    // for a malformed line.
    tl::expected<CgroupDir, CgroupError> ParseCpuCgroupDir(std::string_view mountinfo,
                                                           const CpuCgroup&);

    // From the contents of cgroup v2 cpu.max, "<quota> <period>\n", where quota
    // "max" is Unlimited. MalformedFile for anything else, a negative quota, or
    // a period of zero or less.
    tl::expected<CpuBandwidth, CgroupError> ParseCpuMax(std::string_view cpuMax);

    // From the contents of cgroup v1 cpu.cfs_quota_us and cpu.cfs_period_us,
    // each "<value>\n", where a negative quota is Unlimited. MalformedFile for
    // anything else, or a period of zero or less.
    tl::expected<CpuBandwidth, CgroupError> ParseCfsBandwidth(std::string_view quota,
                                                              std::string_view period);
}
