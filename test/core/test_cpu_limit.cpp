#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_cpu_limit
#else
#define BOOST_TEST_MODULE test_cpu_limitd
#endif

#include <boost/test/unit_test.hpp>

#include <internal/cpu_limit.hpp>

#include <utilstrencodings.h>

#if defined(__linux__)
#include <sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// The helper pool sizes itself from the CPUs this process may use. On Linux a
// container's CPU limit comes from its cgroup. The parsers take file contents,
// so their cases are string tables; CgroupCpuLimit, which only reads the
// files, is checked on a temporary tree, and EffectiveCpuCount on the live
// system.
BOOST_AUTO_TEST_SUITE(container_cpu_limit)

using bsv::internal::CgroupError;
using bsv::internal::CgroupVersion;
using bsv::internal::CpuCgroup;
using enum bsv::internal::CgroupError;

// mountinfo lines as the kernel writes them.
constexpr std::string_view
    ROOT_MOUNT = "22 1 259:1 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p1 rw\n";
constexpr std::string_view
    CGROUP2_MOUNT = "30 23 0:26 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime "
                    "shared:4 - cgroup2 "
                    "cgroup2 rw,nsdelegate,memory_recursiveprot\n";
constexpr std::string_view
    V1_MOUNTS = "40 30 0:35 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/memory "
                "ro,nosuid,nodev,noexec,relatime master:16 - cgroup cgroup "
                "rw,memory\n"
                "41 30 0:36 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/cpu,cpuacct "
                "ro,nosuid,nodev,noexec,relatime master:17 - cgroup "
                "cgroup rw,cpu,cpuacct\n"
                "42 30 0:37 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/cpuset "
                "ro,nosuid,nodev,noexec,relatime master:18 - cgroup cgroup "
                "rw,cpuset\n";
constexpr std::string_view
    V1_CPU_MOUNT = "41 30 0:36 / /sys/fs/cgroup/cpu,cpuacct rw,relatime master:17 - "
                   "cgroup cgroup rw,cpu,cpuacct\n";

static std::string Concat(std::initializer_list<std::string_view> parts)
{
    std::string out;
    for(const std::string_view part : parts)
        out += part;
    return out;
}

// How a parser result reads in a failure message. Limits are compared exactly,
// as hexadecimal floating point; every NaN reads the same.
static std::string Describe(CgroupError error)
{
    switch(error)
    {
    case CgroupError::NoCgroup:
        return "NoCgroup";
    case CgroupError::MalformedFile:
        return "MalformedFile";
    case CgroupError::InvalidEscape:
        return "InvalidEscape";
    case CgroupError::IncompleteLine:
        return "IncompleteLine";
    case CgroupError::SyscallFailed:
        return "SyscallFailed";
    }
    return "CgroupError " + std::to_string(static_cast<int>(error));
}

static std::string Describe(const CpuCgroup& cgroup)
{
    return (cgroup.version == CgroupVersion::V1 ? "v1 " : "v2 ") + cgroup.path;
}

static std::string Describe(const std::string& dir) { return "dir '" + dir + "'"; }

static std::string Describe(double limit)
{
    if(std::isnan(limit))
        return "limit nan";

    std::ostringstream out;
    out << "limit " << std::hexfloat << limit;
    return out.str();
}

template<typename T>
static std::string Describe(const std::variant<T, CgroupError>& result)
{
    return std::visit([](const auto& value) { return Describe(value); }, result);
}

// Each proc/self/cgroup and the cgroup FindCpuCgroup finds in it.
BOOST_AUTO_TEST_CASE(find_cpu_cgroup)
{
    const std::vector<std::pair<std::string, std::variant<CpuCgroup, CgroupError>>> cases{
        {"0::/\n", CpuCgroup{CgroupVersion::V2, "/"}},
        {"0::/system.slice/node.service\n",
         CpuCgroup{CgroupVersion::V2, "/system.slice/node.service"}},
        // Outside the cgroup namespace: FindCpuCgroupDir rejects it, not this.
        {"0::/../../other\n", CpuCgroup{CgroupVersion::V2, "/../../other"}},
        {"12:memory:/kubepods/burstable/pod1/ctr\n11:cpu,cpuacct:/kubepods/burstable/"
         "pod1/ctr\n"
         "10:cpuset:/kubepods/burstable/pod1/ctr\n0::/\n",
         CpuCgroup{CgroupVersion::V1, "/kubepods/burstable/pod1/ctr"}},
        // Only a controller list containing exactly "cpu" counts.
        {"10:cpuset:/kubepods/pod1/ctr\n11:cpu,cpuacct:/kubepods/pod1/ctr\n0::/\n",
         CpuCgroup{CgroupVersion::V1, "/kubepods/pod1/ctr"}},
        {"10:cpuacct:/a\n0::/b\n", CpuCgroup{CgroupVersion::V2, "/b"}},
        // The v1 cpu hierarchy wins over v2 wherever it is listed, and the
        // first one counts.
        {"0::/b\n11:cpu,cpuacct:/a\n", CpuCgroup{CgroupVersion::V1, "/a"}},
        {"11:cpu,cpuacct:/a\n12:cpu:/b\n", CpuCgroup{CgroupVersion::V1, "/a"}},
        // v2 is hierarchy 0 exactly; other hierarchies without cpu don't count,
        // and neither does an empty file.
        {"5::/x\n", NoCgroup},
        {"10:cpuacct:/a\n", NoCgroup},
        {"0123:cpuacct:/a\n", NoCgroup},
        {"", NoCgroup},
        // Malformed: a line without two colons, a blank line, a path not
        // starting with '/'. Each rejects the file even when a valid line
        // follows.
        {"not a cgroup line\n", MalformedFile},
        {"/a/b\n0::/\n", MalformedFile},
        {"0:cpu\n0::/\n", MalformedFile},
        {"\n0::/\n", MalformedFile},
        {"0::relative\n", MalformedFile},
        {"0::relative\n0::/\n", MalformedFile},
        // A last line without its newline is incomplete, whatever it holds,
        // unless the v1 cpu hierarchy comes first.
        {"0::/", IncompleteLine},
        {"0:cpu", IncompleteLine},
        {"11:cpu:/a\n0::/", CpuCgroup{CgroupVersion::V1, "/a"}},
    };
    for(const auto& [cgroupFile, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cgroup file '" << cgroupFile << "'")
        {
            BOOST_CHECK_EQUAL(Describe(expected),
                              Describe(bsv::internal::FindCpuCgroup(cgroupFile)));
        }
    }
}

// Each proc/self/mountinfo and cgroup, and the directory FindCpuCgroupDir
// builds from them.
BOOST_AUTO_TEST_CASE(find_cpu_cgroup_dir)
{
    const CpuCgroup v2Root{CgroupVersion::V2, "/"};
    const std::vector<
        std::tuple<std::string, CpuCgroup, std::variant<std::string, CgroupError>>>
        cases{
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}), v2Root, "/sys/fs/cgroup"},
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}),
             {CgroupVersion::V2, "/system.slice/node.service"},
             "/sys/fs/cgroup/system.slice/node.service"},
            // A space in the mount point is escaped as \040.
            {Concat({ROOT_MOUNT,
                     "30 23 0:26 / /sys/fs/cgroup\\040unified "
                     "rw,nosuid,nodev,noexec,relatime shared:4 - cgroup2 cgroup2 rw\n"}),
             v2Root,
             "/sys/fs/cgroup unified"},
            // A cgroup shown as /../.. lies outside the mount this namespace sees.
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}),
             {CgroupVersion::V2, "/../../other"},
             NoCgroup},
            // A mount rooted at /kubepods/podA does not contain /kubepods/podAB/ctr;
            // the next mount, rooted at /, does.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /kubepods/podA /sys/fs/cgroup-a rw,relatime shared:5 - "
                     "cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             {CgroupVersion::V2, "/kubepods/podAB/ctr"},
             "/sys/fs/cgroup/kubepods/podAB/ctr"},
            // A mount rooted at the cgroup itself.
            {Concat({ROOT_MOUNT, V1_MOUNTS}),
             {CgroupVersion::V1, "/kubepods/burstable/pod1/ctr"},
             "/sys/fs/cgroup/cpu,cpuacct"},
            // v1: only a cgroup mount whose super options contain "cpu" counts.
            {Concat({ROOT_MOUNT,
                     "42 30 0:37 /kubepods/pod1/ctr /sys/fs/cgroup/cpuset "
                     "ro,nosuid,nodev,noexec,relatime master:18 - cgroup cgroup "
                     "rw,cpuset\n"
                     "41 30 0:36 /kubepods/pod1/ctr /sys/fs/cgroup/cpu,cpuacct "
                     "ro,nosuid,nodev,noexec,relatime master:17 - cgroup "
                     "cgroup rw,cpu,cpuacct\n"}),
             {CgroupVersion::V1, "/kubepods/pod1/ctr"},
             "/sys/fs/cgroup/cpu,cpuacct"},
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}), {CgroupVersion::V1, "/"}, NoCgroup},
            // A malformed line (no " - " separator, or no filesystem type after
            // it), a blank line, a bad \ooo escape in the selected mount point, or
            // an escape above \377 in the root of a cgroup2 mount makes the file
            // unreadable even when a valid cgroup2 mount follows.
            {Concat({ROOT_MOUNT, "31 23 0:27 / /mnt/broken rw\n", CGROUP2_MOUNT}),
             v2Root,
             MalformedFile},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /sys/fs/cgroup-t rw,relatime shared:5 - cgroup2\n",
                     CGROUP2_MOUNT}),
             v2Root,
             MalformedFile},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /sys/fs/cgroup\\09x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             InvalidEscape},
            {Concat({ROOT_MOUNT, "\n", CGROUP2_MOUNT}), v2Root, MalformedFile},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x\\777 /sys/fs/cgroup-x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             InvalidEscape},
            // A mount whose root does not start with '/', cgroup or not, rejects
            // the file; an nsfs bind mount is enough, with or without a cgroup2
            // mount after it.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 kubepods /sys/fs/cgroup-k rw,relatime shared:5 - "
                     "cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             MalformedFile},
            {Concat({ROOT_MOUNT,
                     "41 25 0:4 net:[4026532296] /run/netns/ns1 rw shared:12 - nsfs nsfs "
                     "rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             MalformedFile},
            {Concat({ROOT_MOUNT,
                     "41 25 0:4 net:[4026532296] /run/netns/ns1 rw shared:12 - nsfs nsfs "
                     "rw\n"}),
             v2Root,
             MalformedFile},
            // Only the selected mount's mount point is unescaped: a bad escape on
            // a cgroup2 mount whose root does not contain the cgroup is not read.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /elsewhere /mnt/bad\\09x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             {CgroupVersion::V2, "/kubepods/ctr"},
             "/sys/fs/cgroup/kubepods/ctr"},
            // A cgroup2 mount needs its filesystem type and the space after it,
            // not the source or super options. The last line needs no newline.
            {"22 1 259:1 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p1\n"
             "30 23 0:26 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime shared:4 - "
             "cgroup2 cgroup2\n",
             v2Root,
             "/sys/fs/cgroup"},
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT.substr(0, CGROUP2_MOUNT.size() - 1)}),
             v2Root,
             "/sys/fs/cgroup"},
            {std::string(ROOT_MOUNT), v2Root, NoCgroup},
            // A V2 cgroup with v1 cgroup mounts listed first (a hybrid host).
            {Concat({ROOT_MOUNT, V1_CPU_MOUNT, CGROUP2_MOUNT}),
             {CgroupVersion::V2, "/x"},
             "/sys/fs/cgroup/x"},
            // A v1 cgroup mount needs its source; without one the file is
            // malformed, even when the mount is not the cpu one.
            {Concat({ROOT_MOUNT,
                     "40 30 0:35 / /sys/fs/cgroup/z rw - cgroup\n",
                     V1_CPU_MOUNT}),
             {CgroupVersion::V1, "/x"},
             MalformedFile},
            {Concat({ROOT_MOUNT,
                     "40 30 0:35 / /sys/fs/cgroup/z rw - cgroup cgroup\n",
                     V1_CPU_MOUNT}),
             {CgroupVersion::V1, "/x"},
             MalformedFile},
            // Roots are unescaped only on mounts of the wanted type: a bad escape
            // on an ext4 root is not read.
            {Concat({"22 1 259:1 /a\\777 / rw - ext4 /dev/x rw\n", CGROUP2_MOUNT}),
             v2Root,
             "/sys/fs/cgroup"},
            // A mount that can only reach the cgroup through /.. is skipped, and
            // the next one matches.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x /m1 rw - cgroup2 cgroup2 rw\n32 23 0:28 / /m2 rw - "
                     "cgroup2 cgroup2 rw\n"}),
             {CgroupVersion::V2, "/x/../../y"},
             "/m2/x/../../y"},
            // /.. is matched on a path boundary: a directory named ..foo is inside.
            {Concat({ROOT_MOUNT, "31 23 0:27 /x /m1 rw - cgroup2 cgroup2 rw\n"}),
             {CgroupVersion::V2, "/x/..foo"},
             "/m1/..foo"},
            // The first matching mount wins.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /m1 rw - cgroup2 cgroup2 rw\n32 23 0:28 / /m2 rw - "
                     "cgroup2 cgroup2 rw\n"}),
             {CgroupVersion::V2, "/c"},
             "/m1/c"},
            // A root matches as a prefix of the cgroup, not by its length alone.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /aaa /m1 rw - cgroup2 cgroup2 rw\n32 23 0:28 / /m2 rw - "
                     "cgroup2 cgroup2 rw\n"}),
             {CgroupVersion::V2, "/bbb/ctr"},
             "/m2/bbb/ctr"},
            // \ooo escapes: octal digits only, at most \377.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /m\\089 rw - cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             InvalidEscape},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /m\\0/0 rw - cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             InvalidEscape},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x\\400 /m rw - cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             InvalidEscape},
            {Concat({ROOT_MOUNT, "31 23 0:27 / /m\\377 rw - cgroup2 cgroup2 rw\n"}),
             v2Root,
             "/m\xff"},
            // An escaped root that contains the cgroup: \134 is a backslash.
            {Concat({ROOT_MOUNT, "31 23 0:27 /a\\134b /m rw - cgroup2 cgroup2 rw\n"}),
             {CgroupVersion::V2, "/a\\b/ctr"},
             "/m/ctr"},
    };
    for(const auto& [mountinfo, cgroup, expected] : cases)
    {
        BOOST_TEST_CONTEXT(
            "cgroup " << cgroup.path << ", mountinfo '" << mountinfo << "'")
        {
            BOOST_CHECK_EQUAL(Describe(expected),
                              Describe(
                                  bsv::internal::FindCpuCgroupDir(mountinfo, cgroup)));
        }
    }
}

// Each cpu.max and the limit ParseV2Limit reads from it. Every
// limit is quota / period of decimal integers, so it compares exactly.
BOOST_AUTO_TEST_CASE(parse_v2_limit)
{
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<std::pair<std::string, std::variant<double, CgroupError>>> cases{
        {"250000 100000\n", 2.5},
        {"150000 100000\n", 1.5},
        {"120000 100000\n", 1.2},
        {"300000 100000\n", 3.0},
        // "max" is no limit, +Inf, checked before the period and its newline;
        // a file without the space is malformed.
        {"max 100000\n", inf},
        {"max 100000", inf},
        {"max\n", MalformedFile},
        {"two and a half\n", MalformedFile},
        // A zero quota is a limit, which the rounding raises to 2. A negative
        // one too.
        {"0 100000\n", 0.0},
        {"-1 100000\n", -1e-05},
        // A zero period divides as float64 does; the kernel's minimum is 1 ms.
        {"250000 0\n", inf},
        {"-250000 0\n", -inf},
        {"0 0\n", std::numeric_limits<double>::quiet_NaN()},
        // The period ends at its newline; anything after it is not parsed. The
        // 64-byte limit on the file is the reader's (see the v2 file test).
        {"250000 100000\n00000000000000000000000000000000000000000000000000\n", 2.5},
        {"250000 100000", MalformedFile},
        {"250000 100000\r\n", MalformedFile},
        {"250000  100000\n", MalformedFile},
        {"+250000 +100000\n", 2.5},
        {"", MalformedFile},
    };
    for(const auto& [cpuMax, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cpu.max '" << cpuMax << "'")
        {
            BOOST_CHECK_EQUAL(Describe(expected),
                              Describe(bsv::internal::ParseV2Limit(cpuMax)));
        }
    }
}

// Each cpu.cfs_quota_us and cpu.cfs_period_us, and the limit ParseV1Limit
// reads from them.
BOOST_AUTO_TEST_CASE(parse_v1_limit)
{
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<
        std::tuple<std::string, std::string, std::variant<double, CgroupError>>>
        cases{
            {"200000\n", "100000\n", 2.0},
            {"50000\n", "100000\n", 0.5},
            // A negative quota, usually -1, is no limit, +Inf, whatever the
            // period.
            {"-1\n", "100000\n", inf},
            {"-1\n", "garbage", inf},
            // A zero quota is a limit, which the rounding raises to 2.
            {"0\n", "100000\n", 0.0},
            {"200000\n", "0\n", inf},
            {"200000", "100000\n", MalformedFile},
            {"-1", "100000\n", MalformedFile},
            {"200000\n", "100000", MalformedFile},
            {"200000\r\n", "100000\n", MalformedFile},
            {"max\n", "100000\n", MalformedFile},
    };
    for(const auto& [quota, period, expected] : cases)
    {
        BOOST_TEST_CONTEXT("quota '" << quota << "', period '" << period << "'")
        {
            BOOST_CHECK_EQUAL(Describe(expected),
                              Describe(bsv::internal::ParseV1Limit(quota, period)));
        }
    }
}

namespace
{

    // A directory tree under the system's temporary directory, removed with it.
    class TempRoot
    {
    public:
        TempRoot()
        {
            std::random_device random;
            do
                path_ = std::filesystem::temp_directory_path()
                      / ("bdk_test_cpu_limit_" + std::to_string(random()));
            while(!std::filesystem::create_directory(path_));
        }

        ~TempRoot()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        TempRoot(const TempRoot&) = delete;
        TempRoot& operator=(const TempRoot&) = delete;
        TempRoot(TempRoot&&) = delete;
        TempRoot& operator=(TempRoot&&) = delete;

        [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

        void Write(const std::filesystem::path& relative,
                   const std::string& content) const
        {
            const std::filesystem::path file = path_ / relative;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << content;
        }

    private:
        std::filesystem::path path_;
    };

} // namespace

// CgroupCpuLimit under `root`, as Describe reads it.
static std::string LimitUnder(const std::filesystem::path& root)
{
    return Describe(bsv::internal::CgroupCpuLimit(root));
}

// CgroupCpuLimit reads proc/self/cgroup, proc/self/mountinfo and cpu.max under
// the root it is given, the cgroup's directory joined below the root. Each
// file that does not read gives the error Go's runtime reports for it.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_the_v2_files)
{
    const TempRoot root;
    root.Write("proc/self/cgroup", "0::/ctr\n");
    root.Write("proc/self/mountinfo", Concat({ROOT_MOUNT, CGROUP2_MOUNT}));
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n");
    BOOST_CHECK_EQUAL(Describe(1.5), LimitUnder(root.Path()));
#if defined(__linux__)
    // EffectiveCpuCount applies the limit under the root it is given.
    const size_t unlimited = bsv::internal::EffectiveCpuCount(root.Path() / "missing");
    BOOST_CHECK_EQUAL(bsv::internal::LimitCpuCount(unlimited, 1.5),
                      bsv::internal::EffectiveCpuCount(root.Path()));
#endif
    root.Write("sys/fs/cgroup/ctr/cpu.max", "max 100000\n");
    BOOST_CHECK_EQUAL(Describe(std::numeric_limits<double>::infinity()),
                      LimitUnder(root.Path()));
    // A value file of 63 bytes is parsed, one of 64 is not. An empty one reads
    // and does not parse.
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n" + std::string(49, '0'));
    BOOST_CHECK_EQUAL(Describe(1.5), LimitUnder(root.Path()));
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n" + std::string(50, '0'));
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    root.Write("sys/fs/cgroup/ctr/cpu.max", "");
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    // A missing cpu.max does not open; a directory opens but does not read.
    std::filesystem::remove(root.Path() / "sys/fs/cgroup/ctr/cpu.max");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
    std::filesystem::create_directory(root.Path() / "sys/fs/cgroup/ctr/cpu.max");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
    // The cgroup and mountinfo files: missing is no cgroup; empty parses to no
    // cgroup; a directory does not read. The parsers' errors come through.
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path() / "missing"));
    root.Write("proc/self/cgroup", "");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    root.Write("proc/self/cgroup", "0::/ctr");
    BOOST_CHECK_EQUAL(Describe(IncompleteLine), LimitUnder(root.Path()));
    std::filesystem::remove(root.Path() / "proc/self/cgroup");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    std::filesystem::create_directory(root.Path() / "proc/self/cgroup");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
    std::filesystem::remove(root.Path() / "proc/self/cgroup");
    root.Write("proc/self/cgroup", "0::/ctr\n");
    root.Write("proc/self/mountinfo", "");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    root.Write("proc/self/mountinfo", "garbage\n");
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    std::filesystem::remove(root.Path() / "proc/self/mountinfo");
    std::filesystem::create_directory(root.Path() / "proc/self/mountinfo");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
    // As in Go, the cgroup file is read and parsed before mountinfo is opened.
    std::filesystem::remove(root.Path() / "proc/self/cgroup");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    root.Write("proc/self/cgroup", "0::/ctr");
    BOOST_CHECK_EQUAL(Describe(IncompleteLine), LimitUnder(root.Path()));
}

// For cgroup v1, CgroupCpuLimit reads cpu.cfs_quota_us and cpu.cfs_period_us
// from the cpu hierarchy's directory. As in Go, both files are opened first,
// either one that does not read is malformed, and the period is not read for a
// quota that is no limit.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_the_v1_files)
{
    const double inf = std::numeric_limits<double>::infinity();
    const TempRoot root;
    const std::filesystem::path dir = "sys/fs/cgroup/cpu,cpuacct/ctr";
    root.Write("proc/self/cgroup", "11:cpu,cpuacct:/ctr\n0::/\n");
    root.Write("proc/self/mountinfo", Concat({ROOT_MOUNT, V1_CPU_MOUNT}));
    root.Write(dir / "cpu.cfs_quota_us", "50000\n");
    root.Write(dir / "cpu.cfs_period_us", "100000\n");
    BOOST_CHECK_EQUAL(Describe(0.5), LimitUnder(root.Path()));
    // Both files have the 64-byte limit: 63 bytes are parsed, 64 are not.
    root.Write(dir / "cpu.cfs_quota_us", "50000\n" + std::string(57, '0'));
    BOOST_CHECK_EQUAL(Describe(0.5), LimitUnder(root.Path()));
    root.Write(dir / "cpu.cfs_quota_us", "50000\n" + std::string(58, '0'));
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    root.Write(dir / "cpu.cfs_quota_us", "50000\n");
    root.Write(dir / "cpu.cfs_period_us", "100000\n" + std::string(56, '0'));
    BOOST_CHECK_EQUAL(Describe(0.5), LimitUnder(root.Path()));
    root.Write(dir / "cpu.cfs_period_us", "100000\n" + std::string(57, '0'));
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    // ... unless the quota is no limit, as the period is then not read.
    root.Write(dir / "cpu.cfs_quota_us", "-1\n");
    BOOST_CHECK_EQUAL(Describe(inf), LimitUnder(root.Path()));
    // A value file that does not read is malformed.
    std::filesystem::remove(root.Path() / dir / "cpu.cfs_period_us");
    std::filesystem::create_directory(root.Path() / dir / "cpu.cfs_period_us");
    BOOST_CHECK_EQUAL(Describe(inf), LimitUnder(root.Path()));
    root.Write(dir / "cpu.cfs_quota_us", "50000\n");
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    std::filesystem::remove(root.Path() / dir / "cpu.cfs_period_us");
    root.Write(dir / "cpu.cfs_period_us", "100000\n");
    std::filesystem::remove(root.Path() / dir / "cpu.cfs_quota_us");
    std::filesystem::create_directory(root.Path() / dir / "cpu.cfs_quota_us");
    BOOST_CHECK_EQUAL(Describe(MalformedFile), LimitUnder(root.Path()));
    // A missing file does not open, even next to a quota that is no limit.
    std::filesystem::remove(root.Path() / dir / "cpu.cfs_quota_us");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
    root.Write(dir / "cpu.cfs_quota_us", "-1\n");
    std::filesystem::remove(root.Path() / dir / "cpu.cfs_period_us");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
}

// CgroupCpuLimit reads the cgroup and mountinfo files of proc/self, or, when
// proc/self/mountinfo does not read (once the main thread has exited), both of
// proc/thread-self. It never takes one file from each: the cgroup directories that a mix
// would select hold their own limits.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_self_else_thread_self)
{
    const TempRoot root;
    root.Write("proc/self/cgroup", "0::/process\n");
    root.Write("proc/self/mountinfo",
               Concat(
                   {ROOT_MOUNT, "31 23 0:26 / /mnt/cgroup rw - cgroup2 cgroup2 rw\n"}));
    root.Write("proc/thread-self/cgroup", "0::/thread\n");
    root.Write("proc/thread-self/mountinfo", Concat({ROOT_MOUNT, CGROUP2_MOUNT}));
    root.Write("mnt/cgroup/process/cpu.max", "50000 100000\n");
    root.Write("sys/fs/cgroup/thread/cpu.max", "150000 100000\n");
    root.Write("mnt/cgroup/thread/cpu.max", "250000 100000\n");
    root.Write("sys/fs/cgroup/process/cpu.max", "350000 100000\n");
    BOOST_CHECK_EQUAL(Describe(0.5), LimitUnder(root.Path()));
    // proc/self/mountinfo reads, so its cgroup file is the one.
    std::filesystem::remove(root.Path() / "proc/self/cgroup");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    root.Write("proc/self/cgroup", "0::/process\n");
    std::filesystem::remove(root.Path() / "proc/self/mountinfo");
    BOOST_CHECK_EQUAL(Describe(1.5), LimitUnder(root.Path()));
    std::filesystem::remove(root.Path() / "proc/thread-self/cgroup");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    root.Write("proc/thread-self/cgroup", "0::/thread\n");
    // When neither mountinfo reads, the error is proc/self's.
    std::filesystem::remove(root.Path() / "proc/thread-self/mountinfo");
    BOOST_CHECK_EQUAL(Describe(NoCgroup), LimitUnder(root.Path()));
    std::filesystem::create_directory(root.Path() / "proc/self/mountinfo");
    BOOST_CHECK_EQUAL(Describe(SyscallFailed), LimitUnder(root.Path()));
}

// On this machine, whatever its cgroup: at least one CPU. On Linux this reads
// the live affinity mask and cgroup, which hardware_concurrency() need not
// bound (musl counts the calling thread's mask, and only up to 1024 CPUs);
// elsewhere it is hardware_concurrency().
BOOST_AUTO_TEST_CASE(effective_cpu_count_of_this_machine)
{
    const size_t effective = bsv::internal::EffectiveCpuCount();
    const size_t hardware = std::thread::hardware_concurrency();
    BOOST_TEST_MESSAGE("effective="
                       << effective
                       << " hardware_concurrency="
                       << hardware
                       << " cgroup_limit="
                       << Describe(bsv::internal::CgroupCpuLimit("/")));
    BOOST_CHECK_GE(effective, 1U);
#if !defined(__linux__)
    BOOST_CHECK_EQUAL(std::max<size_t>(hardware, 1), effective);
#endif
}

#if defined(__linux__)
// EffectiveCpuCount respects the process's affinity mask, which
// hardware_concurrency() ignores. On Linux that is the main thread's mask, and
// Boost.Test runs test cases on the main thread, so pinning it here narrows
// the mask EffectiveCpuCount reads.
BOOST_AUTO_TEST_CASE(effective_cpu_count_applies_the_affinity_mask)
{
    cpu_set_t original;
    CPU_ZERO(&original);
    BOOST_REQUIRE_EQUAL(0, sched_getaffinity(0, sizeof(original), &original));
    int pinned = -1;
    for(int cpu = 0; cpu < CPU_SETSIZE && pinned < 0; ++cpu)
        if(CPU_ISSET(cpu, &original))
            pinned = cpu;
    BOOST_REQUIRE_GE(pinned, 0);
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(pinned, &one);

    class RestoreAffinity
    {
    public:
        explicit RestoreAffinity(const cpu_set_t& mask): mask_{mask} {}

        ~RestoreAffinity() { sched_setaffinity(0, sizeof(mask_), &mask_); }

        RestoreAffinity(const RestoreAffinity&) = delete;
        RestoreAffinity& operator=(const RestoreAffinity&) = delete;
        RestoreAffinity(RestoreAffinity&&) = delete;
        RestoreAffinity& operator=(RestoreAffinity&&) = delete;

    private:
        cpu_set_t mask_;
    };

    const RestoreAffinity restore{original};
    BOOST_REQUIRE_EQUAL(0, sched_setaffinity(0, sizeof(one), &one));
    BOOST_CHECK_EQUAL(1U, bsv::internal::EffectiveCpuCount());
}

// A thread pinned to one CPU still sees the process's CPUs: the count does not
// depend on which thread asks.
BOOST_AUTO_TEST_CASE(effective_cpu_count_ignores_the_calling_threads_mask)
{
    const size_t process = bsv::internal::EffectiveCpuCount();
    int pinResult = -1;
    size_t fromPinnedThread = 0;
    std::thread pinned(
        [&pinResult, &fromPinnedThread]
        {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            if(sched_getaffinity(0, sizeof(mask), &mask) != 0)
                return;

            int first = 0;
            while(first < CPU_SETSIZE && !CPU_ISSET(first, &mask))
                ++first;
            CPU_ZERO(&mask);
            CPU_SET(first, &mask);
            pinResult = sched_setaffinity(0, sizeof(mask), &mask);
            fromPinnedThread = bsv::internal::EffectiveCpuCount();
        });
    pinned.join();
    BOOST_REQUIRE_EQUAL(0, pinResult);
    if(process <= 1)
        BOOST_TEST_MESSAGE(
            "only " << process << " CPU: pinning a thread cannot lower the count here");
    BOOST_CHECK_EQUAL(process, fromPinnedThread);
}

static std::string ReadText(const std::filesystem::path& path)
{
    const std::ifstream in(path, std::ios::binary);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

// The process's state, from proc/self/stat: 'Z' once its main thread has
// exited while other threads run.
static char ProcessState()
{
    const std::string stat = ReadText("/proc/self/stat");
    const size_t paren = stat.rfind(')');
    return paren != std::string::npos && paren + 2 < stat.size() ? stat[paren + 2] : '?';
}

// What the child below reports through its exit status.
constexpr int LIMIT_READ = 0;
constexpr int NO_LIMIT = 1;
constexpr int OTHER_LIMIT = 2;
constexpr int MAIN_THREAD_RUNNING = 3;

// The cgroup limit read from another thread after the main thread has exited:
// proc/self is then the exited thread's, and its mountinfo no longer opens, so
// CgroupCpuLimit reads proc/thread-self's files. The root's proc is the live one; the
// cgroup's directory under the root holds a known limit, so the test does not depend on
// the machine's.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_after_the_main_thread_exits)
{
    const auto found = bsv::internal::FindCpuCgroup(ReadText("/proc/self/cgroup"));
    const auto* cgroup = std::get_if<CpuCgroup>(&found);
    const auto dir = cgroup
                       ? bsv::internal::FindCpuCgroupDir(ReadText("/proc/self/mountinfo"),
                                                         *cgroup)
                       : std::variant<std::string, CgroupError>(NoCgroup);
    const auto* dirPath = std::get_if<std::string>(&dir);
    if(!dirPath)
    {
        BOOST_TEST_MESSAGE(
            "no cgroup directory with a cpu controller here: " << Describe(dir));
        return;
    }

    const TempRoot root;
    std::filesystem::create_directory_symlink("/proc", root.Path() / "proc");
    const std::filesystem::path cgroupDir = std::filesystem::path(*dirPath)
                                                .relative_path();
    if(cgroup->version == CgroupVersion::V2)
    {
        root.Write(cgroupDir / "cpu.max", "150000 100000\n");
    }
    else
    {
        root.Write(cgroupDir / "cpu.cfs_quota_us", "150000\n");
        root.Write(cgroupDir / "cpu.cfs_period_us", "100000\n");
    }
    BOOST_REQUIRE_EQUAL(Describe(1.5), LimitUnder(root.Path()));

    const pid_t child = fork();
    BOOST_REQUIRE_NE(-1, child);
    if(child == 0)
    {
        // The child's only thread is its main thread. A second thread waits
        // for it to exit, reads the limit and exits the child with the result.
        std::thread(
            [path = root.Path()]
            {
                for(int i = 0; i < 1000 && ProcessState() != 'Z'; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if(ProcessState() != 'Z')
                    _exit(MAIN_THREAD_RUNNING);
                const auto limit = bsv::internal::CgroupCpuLimit(path);
                const auto* value = std::get_if<double>(&limit);
                if(!value)
                    _exit(NO_LIMIT);
                _exit(*value == 1.5 ? LIMIT_READ : OTHER_LIMIT);
            })
            .detach();
        // The exit system call ends only the calling thread; pthread_exit
        // would unwind the test runner's stack.
        syscall(SYS_exit, 0);
    }
    int status = 0;
    BOOST_REQUIRE_EQUAL(child, waitpid(child, &status, 0));
    BOOST_REQUIRE(WIFEXITED(status));
    BOOST_CHECK_EQUAL(LIMIT_READ, WEXITSTATUS(status));
}
#endif

// cgroup integers are whole-string base 10 with an optional sign, as ParseInt64
// parses them: anything after the digits, and any surrounding whitespace,
// rejects the value.
BOOST_AUTO_TEST_CASE(cgroup_integers_parse_the_whole_string)
{
    constexpr int64_t max = std::numeric_limits<int64_t>::max();
    constexpr int64_t min = std::numeric_limits<int64_t>::min();
    const std::vector<std::pair<std::string, std::optional<int64_t>>> cases{
        {"100000", 100'000},
        {"0", 0},
        {"-1", -1},
        {"+5", 5},
        {"9223372036854775807", max},
        {"+9223372036854775807", max},
        {"-9223372036854775808", min},
        // Trailing characters
        {"250000x", std::nullopt},
        {"100000 ", std::nullopt},
        {"100000\n", std::nullopt},
        {"1 2", std::nullopt},
        {"12.5", std::nullopt},
        {"0x10", std::nullopt},
        {std::string("5\0", 2), std::nullopt},
        // Leading characters, signs alone or doubled, out of range
        {"", std::nullopt},
        {" 5", std::nullopt},
        {"\t5", std::nullopt},
        {"\xff"
         "5",
         std::nullopt},
        {"+", std::nullopt},
        {"-", std::nullopt},
        {"+-5", std::nullopt},
        {"-+5", std::nullopt},
        {"++5", std::nullopt},
        {"x1", std::nullopt},
        {"9223372036854775808", std::nullopt},
        {"-9223372036854775809", std::nullopt},
    };
    for(const auto& [text, expected] : cases)
    {
        BOOST_TEST_CONTEXT("text '" << text << "'")
        {
            int64_t actual{};
            BOOST_CHECK_EQUAL(expected.has_value(), ParseInt64(text, &actual));
            if(expected)
                BOOST_CHECK_EQUAL(*expected, actual);
        }
    }
}

// The CPUs available to the process, lowered to the cgroup limit rounded up,
// and never below MIN_LIMITED_CPUS because of the limit.
BOOST_AUTO_TEST_CASE(cpu_count_rounds_the_limit_up_to_min_limited_cpus)
{
    using bsv::internal::MIN_LIMITED_CPUS;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr size_t maxCpus = std::numeric_limits<size_t>::max();
    static_assert(MIN_LIMITED_CPUS == 2, "the rows below round to 3");
    // {CPUs, cgroup limit, expected count}
    const std::vector<std::tuple<size_t, double, size_t>> cases{
        {16, 4.0, 4},
        // Rounded up, not to nearest.
        {16, 2.5, 3},
        {16, 2.2, 3},
        {16, 2.00001, 3},
        {16, 15.5, 16},
        // Never below the minimum, whatever the limit.
        {16, 1.01, MIN_LIMITED_CPUS},
        {16, 0.5, MIN_LIMITED_CPUS},
        {16, 0.0, MIN_LIMITED_CPUS},
        {16, -0.5, MIN_LIMITED_CPUS},
        {16, -1.0, MIN_LIMITED_CPUS},
        {16, -inf, MIN_LIMITED_CPUS},
        // +Inf (no limit), NaN and limits too large for a CPU count leave the
        // count unchanged.
        {16, inf, 16},
        {16, nan, 16},
        {16, 1e300, 16},
        {16, std::numeric_limits<double>::max(), 16},
        {maxCpus, inf, maxCpus},
        {maxCpus, nan, maxCpus},
        {maxCpus, 1e9 + 0.5, 1'000'000'001},
        // The limit only lowers the count, the minimum included.
        {4, 8.0, 4},
        {1, 0.5, 1},
        {0, 0.5, 0},
    };
    for(const auto& [cpus, limit, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cpus " << cpus << ", " << Describe(limit))
        {
            BOOST_CHECK_EQUAL(expected, bsv::internal::LimitCpuCount(cpus, limit));
        }
    }
}

// The process's affinity mask decides, not hardware_concurrency():
// on musl that is sched_getaffinity(0), the calling thread's mask, so a
// thread pinned to one CPU sees 1.
BOOST_AUTO_TEST_CASE(cpu_count_takes_the_affinity_mask_over_hardware_concurrency)
{
    using bsv::internal::CpuCountFrom;
    const double inf = std::numeric_limits<double>::infinity();
    BOOST_CHECK_EQUAL(16U, CpuCountFrom(16, 1, inf));
    BOOST_CHECK_EQUAL(4U, CpuCountFrom(4, 16, inf));
    BOOST_CHECK_EQUAL(3U, CpuCountFrom(16, 1, 2.5));
    BOOST_CHECK_EQUAL(16U, CpuCountFrom(std::nullopt, 16, inf));
    BOOST_CHECK_EQUAL(3U, CpuCountFrom(std::nullopt, 16, 2.5));
}

// hardware_concurrency() returns 0 when it cannot tell; the count is still at
// least one CPU.
BOOST_AUTO_TEST_CASE(cpu_count_is_at_least_one)
{
    using bsv::internal::CpuCountFrom;
    const double inf = std::numeric_limits<double>::infinity();
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, inf));
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, 2.5));
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, 0.0));
}

BOOST_AUTO_TEST_SUITE_END()
