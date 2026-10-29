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

using bsv::internal::CgroupVersion;
using bsv::internal::CpuCgroup;

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

// Each proc/self/cgroup and the cgroup Go 1.26's parseCPUCgroup finds in it.
BOOST_AUTO_TEST_CASE(find_cpu_cgroup)
{
    const std::vector<std::pair<std::string, std::optional<CpuCgroup>>> cases{
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
        // v2 is hierarchy 0 exactly; other hierarchies without cpu don't count.
        {"5::/x\n", std::nullopt},
        {"10:cpuacct:/a\n", std::nullopt},
        {"0123:cpuacct:/a\n", std::nullopt},
        // Malformed: a line without two colons, a blank line, a last line
        // without its newline, a path not starting with '/', nothing at all.
        // Each rejects the file even when a valid line follows.
        {"not a cgroup line\n", std::nullopt},
        {"/a/b\n0::/\n", std::nullopt},
        {"\n0::/\n", std::nullopt},
        {"0::/", std::nullopt},
        {"0::relative\n", std::nullopt},
        {"0::relative\n0::/\n", std::nullopt},
        {"", std::nullopt},
    };
    for(const auto& [cgroupFile, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cgroup file '" << cgroupFile << "'")
        {
            const auto actual = bsv::internal::FindCpuCgroup(cgroupFile);
            BOOST_CHECK_EQUAL(expected.has_value(), actual.has_value());
            if(expected && actual)
            {
                BOOST_CHECK(expected->version == actual->version);
                BOOST_CHECK_EQUAL(expected->path, actual->path);
            }
        }
    }
}

// Each proc/self/mountinfo and cgroup, and the directory Go 1.26's
// parseCPUMount builds from them.
BOOST_AUTO_TEST_CASE(find_cpu_cgroup_dir)
{
    const CpuCgroup v2Root{CgroupVersion::V2, "/"};
    const std::vector<std::tuple<std::string, CpuCgroup, std::optional<std::string>>>
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
             std::nullopt},
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
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}), {CgroupVersion::V1, "/"}, std::nullopt},
            // A malformed line (no " - " separator, or no filesystem type after
            // it), a blank line, a bad \ooo escape in the selected mount point, or
            // an escape above \377 in the root of a cgroup2 mount makes the file
            // unreadable even when a valid cgroup2 mount follows, as in Go.
            {Concat({ROOT_MOUNT, "31 23 0:27 / /mnt/broken rw\n", CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /sys/fs/cgroup-t rw,relatime shared:5 - cgroup2\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /sys/fs/cgroup\\09x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            {Concat({ROOT_MOUNT, "\n", CGROUP2_MOUNT}), v2Root, std::nullopt},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x\\777 /sys/fs/cgroup-x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            // Go rejects mountinfo when the root of any mount, not only a cgroup
            // mount, does not start with '/'; an nsfs bind mount is enough.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 kubepods /sys/fs/cgroup-k rw,relatime shared:5 - "
                     "cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "41 25 0:4 net:[4026532296] /run/netns/ns1 rw shared:12 - nsfs nsfs "
                     "rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
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
            {std::string(ROOT_MOUNT), v2Root, std::nullopt},
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
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "40 30 0:35 / /sys/fs/cgroup/z rw - cgroup cgroup\n",
                     V1_CPU_MOUNT}),
             {CgroupVersion::V1, "/x"},
             std::nullopt},
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
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /m\\0/0 rw - cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x\\400 /m rw - cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             std::nullopt},
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
            const auto actual = bsv::internal::FindCpuCgroupDir(mountinfo, cgroup);
            BOOST_CHECK_EQUAL(expected.has_value(), actual.has_value());
            if(expected && actual)
                BOOST_CHECK_EQUAL(*expected, *actual);
        }
    }
}

// Each cpu.max and the limit Go 1.26's parseV2Limit reads from it. Every
// limit is quota / period of decimal integers, so it compares exactly.
BOOST_AUTO_TEST_CASE(parse_v2_limit)
{
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<std::pair<std::string, std::optional<double>>> cases{
        {"250000 100000\n", 2.5},
        {"150000 100000\n", 1.5},
        {"120000 100000\n", 1.2},
        {"300000 100000\n", 3.0},
        {"max 100000\n", std::nullopt},
        {"max\n", std::nullopt},
        {"two and a half\n", std::nullopt},
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
        {"250000 100000", std::nullopt},
        {"250000 100000\r\n", std::nullopt},
        {"250000  100000\n", std::nullopt},
        {"+250000 +100000\n", 2.5},
        {"", std::nullopt},
    };
    for(const auto& [cpuMax, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cpu.max '" << cpuMax << "'")
        {
            const auto actual = bsv::internal::ParseV2Limit(cpuMax);
            BOOST_CHECK_EQUAL(expected.has_value(), actual.has_value());
            if(expected && actual && std::isnan(*expected))
                BOOST_CHECK(std::isnan(*actual));
            else if(expected && actual)
                BOOST_CHECK_EQUAL(*expected, *actual);
        }
    }
}

// Each cpu.cfs_quota_us and cpu.cfs_period_us, and the limit Go 1.26 reads
// from them (parseV1Number on each, then ReadCPULimit).
BOOST_AUTO_TEST_CASE(parse_v1_limit)
{
    const std::vector<std::tuple<std::string, std::string, std::optional<double>>> cases{
        {"200000\n", "100000\n", 2.0},
        {"50000\n", "100000\n", 0.5},
        // A negative quota, usually -1, is no limit, whatever the period.
        {"-1\n", "100000\n", std::nullopt},
        {"-1\n", "garbage", std::nullopt},
        // A zero quota is a limit, which the rounding raises to 2.
        {"0\n", "100000\n", 0.0},
        {"200000\n", "0\n", std::numeric_limits<double>::infinity()},
        {"200000", "100000\n", std::nullopt},
        {"200000\n", "100000", std::nullopt},
        {"200000\r\n", "100000\n", std::nullopt},
        {"max\n", "100000\n", std::nullopt},
    };
    for(const auto& [quota, period, expected] : cases)
    {
        BOOST_TEST_CONTEXT("quota '" << quota << "', period '" << period << "'")
        {
            const auto actual = bsv::internal::ParseV1Limit(quota, period);
            BOOST_CHECK_EQUAL(expected.has_value(), actual.has_value());
            if(expected && actual)
                BOOST_CHECK_EQUAL(*expected, *actual);
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

// CgroupCpuLimit reads proc/self/cgroup, proc/self/mountinfo and cpu.max under
// the root it is given, the cgroup's directory joined below the root.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_the_v2_files)
{
    using bsv::internal::CgroupCpuLimit;
    const TempRoot root;
    root.Write("proc/self/cgroup", "0::/ctr\n");
    root.Write("proc/self/mountinfo", Concat({ROOT_MOUNT, CGROUP2_MOUNT}));
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n");
    BOOST_CHECK_EQUAL(1.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
#if defined(__linux__)
    // EffectiveCpuCount applies the limit under the root it is given.
    const size_t unlimited = bsv::internal::EffectiveCpuCount(root.Path() / "missing");
    BOOST_CHECK_EQUAL(bsv::internal::LimitCpuCount(unlimited, 1.5),
                      bsv::internal::EffectiveCpuCount(root.Path()));
#endif
    // Go reads a value file with one 64-byte pread and rejects it if that
    // fills the buffer: 63 bytes are parsed, 64 are not.
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n" + std::string(49, '0'));
    BOOST_CHECK_EQUAL(1.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    root.Write("sys/fs/cgroup/ctr/cpu.max", "150000 100000\n" + std::string(50, '0'));
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
    std::filesystem::remove(root.Path() / "sys/fs/cgroup/ctr/cpu.max");
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
    // A directory opens but does not read.
    std::filesystem::create_directory(root.Path() / "sys/fs/cgroup/ctr/cpu.max");
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
    BOOST_CHECK(!CgroupCpuLimit(root.Path() / "missing").has_value());
}

// For cgroup v1, CgroupCpuLimit reads cpu.cfs_quota_us and cpu.cfs_period_us
// from the cpu hierarchy's directory.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_the_v1_files)
{
    using bsv::internal::CgroupCpuLimit;
    const TempRoot root;
    root.Write("proc/self/cgroup", "11:cpu,cpuacct:/ctr\n0::/\n");
    root.Write("proc/self/mountinfo",
               Concat({ROOT_MOUNT,
                       "41 30 0:36 / /sys/fs/cgroup/cpu,cpuacct rw,relatime master:17 - "
                       "cgroup cgroup rw,cpu,cpuacct\n"}));
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_quota_us", "50000\n");
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_period_us", "100000\n");
    BOOST_CHECK_EQUAL(0.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    // Both files have the 64-byte limit: 63 bytes are parsed, 64 are not.
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_quota_us",
               "50000\n" + std::string(57, '0'));
    BOOST_CHECK_EQUAL(0.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_quota_us",
               "50000\n" + std::string(58, '0'));
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_quota_us", "50000\n");
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_period_us",
               "100000\n" + std::string(56, '0'));
    BOOST_CHECK_EQUAL(0.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    root.Write("sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_period_us",
               "100000\n" + std::string(57, '0'));
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
    std::filesystem::remove(root.Path()
                            / "sys/fs/cgroup/cpu,cpuacct/ctr/cpu.cfs_period_us");
    BOOST_CHECK(!CgroupCpuLimit(root.Path()).has_value());
}

// CgroupCpuLimit reads the cgroup and mountinfo files of proc/thread-self, and
// each one it cannot read there from proc/self, as on kernels before 3.17.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_reads_thread_self_then_self)
{
    using bsv::internal::CgroupCpuLimit;
    const TempRoot root;
    root.Write("proc/thread-self/cgroup", "0::/thread\n");
    root.Write("proc/thread-self/mountinfo", Concat({ROOT_MOUNT, CGROUP2_MOUNT}));
    root.Write("proc/self/cgroup", "0::/process\n");
    root.Write("proc/self/mountinfo",
               Concat(
                   {ROOT_MOUNT, "31 23 0:26 / /mnt/cgroup rw - cgroup2 cgroup2 rw\n"}));
    root.Write("sys/fs/cgroup/thread/cpu.max", "150000 100000\n");
    root.Write("mnt/cgroup/thread/cpu.max", "250000 100000\n");
    root.Write("mnt/cgroup/process/cpu.max", "50000 100000\n");
    BOOST_CHECK_EQUAL(1.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    std::filesystem::remove(root.Path() / "proc/thread-self/mountinfo");
    BOOST_CHECK_EQUAL(2.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
    std::filesystem::remove(root.Path() / "proc/thread-self/cgroup");
    BOOST_CHECK_EQUAL(0.5, CgroupCpuLimit(root.Path()).value_or(-1.0));
}

// Go's default GOMAXPROCS rule (runtime/cgroup_linux.go, the same from go1.25.0 on,
// adjustCgroupGOMAXPROCS): the CPUs available to the process, lowered to the
// cgroup limit rounded up, and never below 2 because of the limit.
// On this machine, whatever its cgroup: at least one CPU. On Linux this reads
// the live affinity mask and cgroup, which hardware_concurrency() need not
// bound (musl counts the calling thread's mask, and only up to 1024 CPUs);
// elsewhere it is hardware_concurrency().
BOOST_AUTO_TEST_CASE(effective_cpu_count_of_this_machine)
{
    const size_t effective = bsv::internal::EffectiveCpuCount();
    const size_t hardware = std::thread::hardware_concurrency();
    const auto limit = bsv::internal::CgroupCpuLimit("/");
    BOOST_TEST_MESSAGE("effective="
                       << effective
                       << " hardware_concurrency="
                       << hardware
                       << " cgroup_limit="
                       << (limit ? std::to_string(*limit) : std::string("none")));
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
// proc/self is then the exited thread's, and its mountinfo no longer opens.
// The root's proc is the live one; the cgroup's directory under the root holds
// a known limit, so the test does not depend on the machine's.
BOOST_AUTO_TEST_CASE(cgroup_cpu_limit_after_the_main_thread_exits)
{
    using bsv::internal::CgroupCpuLimit;
    const auto cgroup = bsv::internal::FindCpuCgroup(ReadText("/proc/self/cgroup"));
    const auto dir = cgroup
                       ? bsv::internal::FindCpuCgroupDir(ReadText("/proc/self/mountinfo"),
                                                         *cgroup)
                       : std::nullopt;
    if(!dir)
    {
        BOOST_TEST_MESSAGE("no cgroup directory with a cpu controller here");
        return;
    }
    const TempRoot root;
    std::filesystem::create_directory_symlink("/proc", root.Path() / "proc");
    const std::filesystem::path cgroupDir = std::filesystem::path(*dir).relative_path();
    if(cgroup->version == CgroupVersion::V2)
    {
        root.Write(cgroupDir / "cpu.max", "150000 100000\n");
    }
    else
    {
        root.Write(cgroupDir / "cpu.cfs_quota_us", "150000\n");
        root.Write(cgroupDir / "cpu.cfs_period_us", "100000\n");
    }
    BOOST_REQUIRE_EQUAL(1.5, CgroupCpuLimit(root.Path()).value_or(-1.0));

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
                const auto limit = CgroupCpuLimit(path);
                if(!limit)
                    _exit(NO_LIMIT);
                _exit(*limit == 1.5 ? LIMIT_READ : OTHER_LIMIT);
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

// cgroup integers are whole-string base 10 with an optional sign, as Go's
// strconv.ParseInt(s, 10, 64), which SV Node's ParseInt64 matches: anything
// after the digits, and any surrounding whitespace, rejects the value.
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

BOOST_AUTO_TEST_CASE(cpu_count_rounds_the_limit_like_gomaxprocs)
{
    const double inf = std::numeric_limits<double>::infinity();
    // {CPUs, cgroup limit, expected count}
    const std::vector<std::tuple<size_t, std::optional<double>, size_t>> cases{
        {16, std::nullopt, 16},
        {16, 4.0, 4},
        // Rounded up, not to nearest.
        {16, 2.5, 3},
        {16, 2.2, 3},
        {16, 2.00001, 3},
        {16, 1.01, 2},
        {16, 0.5, 2},
        {16, 0.0, 2},
        {16, -1.0, 2},
        // Go: max(ceil(-Inf), 2) is 2. Go's int32 conversion of +Inf and NaN
        // is platform-specific; BDK leaves the count unchanged.
        {16, -inf, 2},
        {16, inf, 16},
        {16, std::numeric_limits<double>::quiet_NaN(), 16},
        {4, 8.0, 4},
        {1, 0.5, 1},
    };
    for(const auto& [cpus, limit, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cpus "
                           << cpus
                           << ", limit "
                           << (limit ? std::to_string(*limit) : std::string("none")))
        {
            BOOST_CHECK_EQUAL(expected, bsv::internal::LimitCpuCount(cpus, limit));
        }
    }
}

// The process's affinity mask decides, as in Go, not hardware_concurrency():
// on musl that is sched_getaffinity(0), the calling thread's mask, so a
// thread pinned to one CPU sees 1.
BOOST_AUTO_TEST_CASE(cpu_count_takes_the_affinity_mask_over_hardware_concurrency)
{
    using bsv::internal::CpuCountFrom;
    BOOST_CHECK_EQUAL(16U, CpuCountFrom(16, 1, std::nullopt));
    BOOST_CHECK_EQUAL(4U, CpuCountFrom(4, 16, std::nullopt));
    BOOST_CHECK_EQUAL(3U, CpuCountFrom(16, 1, 2.5));
    BOOST_CHECK_EQUAL(16U, CpuCountFrom(std::nullopt, 16, std::nullopt));
    BOOST_CHECK_EQUAL(3U, CpuCountFrom(std::nullopt, 16, 2.5));
}

// hardware_concurrency() returns 0 when it cannot tell; the count is still at
// least one CPU.
BOOST_AUTO_TEST_CASE(cpu_count_is_at_least_one)
{
    using bsv::internal::CpuCountFrom;
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, std::nullopt));
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, 2.5));
    BOOST_CHECK_EQUAL(1U, CpuCountFrom(std::nullopt, 0, 0.0));
}

BOOST_AUTO_TEST_SUITE_END()
