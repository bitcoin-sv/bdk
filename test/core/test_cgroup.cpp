#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_cgroup
#else
#define BOOST_TEST_MODULE test_cgroupd
#endif

#include <boost/test/unit_test.hpp>

#include "expected_printer.h"
#include "temp_directory.h"

#include <detail/cgroup.hpp>

#include <tl/expected.hpp>

#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// On Linux a container's CPU limit comes from its cgroup. The parsers take
// file contents, so their cases are string tables; FindCgroupDir and
// ReadCpuBandwidth, which read the files, are checked on a temporary tree.
// Cases where BDK differs from Go 1.26's runtime say so. Untested here: Go's
// fixed buffers, which limit lines to 64 KiB and paths to PATH_MAX; the kernel
// writes neither.
BOOST_AUTO_TEST_SUITE(cgroup)

// ReadCpuBandwidth's documented limit on a value file's size.
constexpr size_t VALUE_FILE_SIZE_LIMIT{64};

// The kernel's default period, 100 ms, as cpu.max and cpu.cfs_period_us
// hold it.
constexpr int64_t DEFAULT_PERIOD{100'000};

// mountinfo lines as the kernel writes them.
constexpr std::string_view ROOT_MOUNT{
    "22 1 259:1 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p1 rw\n"};
constexpr std::string_view CGROUP2_MOUNT{
    "30 23 0:26 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime "
    "shared:4 - cgroup2 "
    "cgroup2 rw,nsdelegate,memory_recursiveprot\n"};
constexpr std::string_view V1_MOUNTS{
    "40 30 0:35 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/memory "
    "ro,nosuid,nodev,noexec,relatime master:16 - cgroup cgroup "
    "rw,memory\n"
    "41 30 0:36 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/cpu,cpuacct "
    "ro,nosuid,nodev,noexec,relatime master:17 - cgroup "
    "cgroup rw,cpu,cpuacct\n"
    "42 30 0:37 /kubepods/burstable/pod1/ctr /sys/fs/cgroup/cpuset "
    "ro,nosuid,nodev,noexec,relatime master:18 - cgroup cgroup "
    "rw,cpuset\n"};
constexpr std::string_view V1_CPU_MOUNT{
    "41 30 0:36 / /sys/fs/cgroup/cpu,cpuacct rw,relatime master:17 - "
    "cgroup cgroup rw,cpu,cpuacct\n"};

// The parsers stop at the newline, so the padding is never parsed.
static std::string PaddedTo(const std::string& content, const size_t size)
{
    return content + std::string(size - content.size(), '0');
}

static std::string Concat(std::initializer_list<std::string_view> parts)
{
    std::string out;
    for(const std::string_view part : parts)
        out += part;
    return out;
}

// Files under a root: each relative path with its contents, or nullopt for a
// directory there instead, which opens but does not read.
using Tree = std::vector<std::pair<std::string, std::optional<std::string>>>;

static void WriteTree(const std::filesystem::path& root, const Tree& tree)
{
    for(const auto& [path, content] : tree)
    {
        if(content)
            bsv::test::WriteFile(root / path, *content);
        else
            std::filesystem::create_directories(root / path);
    }
}

// The paths of a tree, for a failing check's context.
static std::string Paths(const Tree& tree)
{
    std::string out;
    for(const auto& [path, content] : tree)
        out += path + (content ? " " : " (directory) ");
    return out.empty() ? "nothing" : out;
}

// FindCgroupDir joins the cgroup's directory, found from proc/self/cgroup and
// proc/self/mountinfo, below the root it is given. A proc file that is missing
// is no cgroup, one that does not read is SyscallFailed, and the parsers'
// errors come through.
BOOST_AUTO_TEST_CASE(find_cgroup_dir_reads_the_proc_files,
                     *boost::unit_test::label("integration"))
{
    using namespace bsv::detail;
    using enum CgroupError;

    const std::string v2Mounts{Concat({ROOT_MOUNT, CGROUP2_MOUNT})};
    const std::string v1Mounts{Concat({ROOT_MOUNT, V1_CPU_MOUNT})};
    // {tree, expected directory relative to the root}
    const std::vector<std::pair<Tree, tl::expected<CgroupDir, CgroupError>>> cases{
        {{{"proc/self/cgroup", "0::/ctr\n"}, {"proc/self/mountinfo", v2Mounts}},
         CgroupDir{CgroupVersion::V2, "sys/fs/cgroup/ctr"}},
        {{{"proc/self/cgroup", "11:cpu,cpuacct:/ctr\n0::/\n"},
          {"proc/self/mountinfo", v1Mounts}},
         CgroupDir{CgroupVersion::V1, "sys/fs/cgroup/cpu,cpuacct/ctr"}},
        {{}, tl::unexpected{NoCgroup}},
        {{{"proc/self/mountinfo", v2Mounts}}, tl::unexpected{NoCgroup}},
        {{{"proc/self/cgroup", std::nullopt}, {"proc/self/mountinfo", v2Mounts}},
         tl::unexpected{SyscallFailed}},
        {{{"proc/self/cgroup", "0::/ctr"}, {"proc/self/mountinfo", v2Mounts}},
         tl::unexpected{IncompleteLine}},
        {{{"proc/self/cgroup", "0::/ctr\n"}, {"proc/self/mountinfo", "garbage\n"}},
         tl::unexpected{MalformedFile}},
        {{{"proc/self/cgroup", "0::/ctr\n"}, {"proc/self/mountinfo", std::nullopt}},
         tl::unexpected{SyscallFailed}},
        // The cgroup file is read, and parsed, before mountinfo.
        {{{"proc/self/mountinfo", std::nullopt}}, tl::unexpected{NoCgroup}},
        {{{"proc/self/cgroup", "0::/ctr"}, {"proc/self/mountinfo", "garbage\n"}},
         tl::unexpected{IncompleteLine}},
    };
    for(const auto& [tree, expected] : cases)
    {
        const bsv::test::TempDirectory root;
        WriteTree(root.Path(), tree);
        const auto expectedDir{expected.transform(
            [&root](const CgroupDir& dir)
            { return CgroupDir{dir.version, root.Path() / dir.path}; })};
        BOOST_TEST_CONTEXT("tree " << Paths(tree))
        {
            BOOST_CHECK_EQUAL(expectedDir, FindCgroupDir(root.Path()));
        }
    }
}

// FindCgroupDir reads the cgroup and mountinfo files of proc/self, or, when
// either does not read (its mountinfo does not once the main thread has
// exited), both of proc/thread-self. It never takes one file from each.
BOOST_AUTO_TEST_CASE(find_cgroup_dir_reads_self_else_thread_self,
                     *boost::unit_test::label("integration"))
{
    using namespace bsv::detail;
    using enum CgroupError;

    const Tree::value_type selfCgroup{"proc/self/cgroup", "0::/process\n"};
    const Tree::value_type
        selfMountinfo{"proc/self/mountinfo",
                      Concat({ROOT_MOUNT,
                              "31 23 0:26 / /mnt/cgroup rw - cgroup2 cgroup2 rw\n"})};
    const Tree::value_type threadCgroup{"proc/thread-self/cgroup", "0::/thread\n"};
    const Tree::value_type threadMountinfo{"proc/thread-self/mountinfo",
                                           Concat({ROOT_MOUNT, CGROUP2_MOUNT})};
    const CgroupDir self{CgroupVersion::V2, "mnt/cgroup/process"};
    const CgroupDir threadSelf{CgroupVersion::V2, "sys/fs/cgroup/thread"};
    // {tree, expected directory relative to the root}
    const std::vector<std::pair<Tree, tl::expected<CgroupDir, CgroupError>>> cases{
        {{selfCgroup, selfMountinfo, threadCgroup, threadMountinfo}, self},
        // When either of proc/self's files does not read, proc/thread-self's
        // are the ones.
        {{selfMountinfo, threadCgroup, threadMountinfo}, threadSelf},
        {{selfCgroup, threadCgroup, threadMountinfo}, threadSelf},
        {{threadCgroup, threadMountinfo}, threadSelf},
        {{selfCgroup, threadMountinfo}, tl::unexpected{NoCgroup}},
        // When neither directory's files read, the error is proc/self's.
        {{selfCgroup, threadCgroup}, tl::unexpected{NoCgroup}},
        {{selfCgroup, {"proc/self/mountinfo", std::nullopt}, threadCgroup},
         tl::unexpected{SyscallFailed}},
    };
    for(const auto& [tree, expected] : cases)
    {
        const bsv::test::TempDirectory root;
        WriteTree(root.Path(), tree);
        const auto expectedDir{expected.transform(
            [&root](const CgroupDir& dir)
            { return CgroupDir{dir.version, root.Path() / dir.path}; })};
        BOOST_TEST_CONTEXT("tree " << Paths(tree))
        {
            BOOST_CHECK_EQUAL(expectedDir, FindCgroupDir(root.Path()));
        }
    }
}

#if defined(__linux__)
static std::string ReadText(const std::filesystem::path& path)
{
    const std::ifstream in(path, std::ios::binary);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

// The process state once the main thread has exited while other threads run.
constexpr char ZOMBIE{'Z'};

// The state field of /proc/self/stat.
static char ProcessState()
{
    // The state follows the command name, which may itself hold ')'.
    constexpr std::string_view BEFORE_STATE{") "};
    const std::string stat{ReadText("/proc/self/stat")};
    const size_t before{stat.rfind(BEFORE_STATE)};
    const size_t state{before + BEFORE_STATE.size()};
    return before != std::string::npos && state < stat.size() ? stat[state] : '?';
}

// What the child below reports through its exit status.
constexpr int DIR_FOUND{0};
constexpr int NO_DIR{1};
constexpr int OTHER_DIR{2};
constexpr int MAIN_THREAD_RUNNING{3};

// The cgroup directory found from another thread after the main thread has
// exited: proc/self is then the exited thread's, and its mountinfo no longer
// opens, so FindCgroupDir reads proc/thread-self's files. The root's proc is the
// live one. Go reads only proc/self, so fails here.
BOOST_AUTO_TEST_CASE(find_cgroup_dir_after_the_main_thread_exits,
                     *boost::unit_test::label("integration"))
{
    using namespace bsv::detail;

    const auto found{ParseCpuCgroup(ReadText("/proc/self/cgroup"))};
    const auto dir{found.and_then(
        [](const CpuCgroup& cgroup)
        { return ParseCpuCgroupDir(ReadText("/proc/self/mountinfo"), cgroup); })};
    if(!dir)
    {
        BOOST_TEST_MESSAGE("no cgroup directory with a cpu controller here: " << dir);
        return;
    }

    const bsv::test::TempDirectory root;
    std::filesystem::create_directory_symlink("/proc", root.Path() / "proc");
    const CgroupDir expected{dir->version, root.Path() / dir->path.relative_path()};
    BOOST_REQUIRE_EQUAL(expected, FindCgroupDir(root.Path()));

    const pid_t child{fork()};
    BOOST_REQUIRE_NE(-1, child);
    if(child == 0)
    {
        // The child's only thread is its main thread. A second thread waits
        // for it to exit, finds the directory and exits the child with the
        // result.
        std::thread(
            [path = root.Path(), expected]
            {
                constexpr auto EXIT_TIMEOUT{std::chrono::seconds(10)};
                constexpr auto POLL_INTERVAL{std::chrono::milliseconds(10)};
                const auto deadline{std::chrono::steady_clock::now() + EXIT_TIMEOUT};
                while(ProcessState() != ZOMBIE
                      && std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(POLL_INTERVAL);
                if(ProcessState() != ZOMBIE)
                    _exit(MAIN_THREAD_RUNNING);
                const auto other{FindCgroupDir(path)};
                if(!other)
                    _exit(NO_DIR);
                _exit(*other == expected ? DIR_FOUND : OTHER_DIR);
            })
            .detach();
        // The exit system call ends only the calling thread; pthread_exit
        // would unwind the test runner's stack.
        syscall(SYS_exit, 0);
    }
    int status{};
    BOOST_REQUIRE_EQUAL(child, waitpid(child, &status, 0));
    BOOST_REQUIRE(WIFEXITED(status));
    BOOST_CHECK_EQUAL(DIR_FOUND, WEXITSTATUS(status));
}
#endif

// A bandwidth's quota is not negative and its period is above zero.
BOOST_AUTO_TEST_CASE(cpu_bandwidth_rejects_a_negative_quota_or_a_period_of_zero_or_less)
{
    using namespace bsv::detail;
    using enum CgroupError;

    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile},
                      CpuBandwidth::Make(-1, DEFAULT_PERIOD));
    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile},
                      CpuBandwidth::Make(std::numeric_limits<int64_t>::min(), 1));
    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile}, CpuBandwidth::Make(100'000, 0));
    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile}, CpuBandwidth::Make(Unlimited{}, -1));
    BOOST_CHECK(CpuBandwidth::Make(0, 1).has_value());
    const CpuBandwidth bandwidth{CpuBandwidth::Make(50'000, 1).value()};
    BOOST_CHECK(bandwidth.Quota() == CpuQuota{50'000});
    BOOST_CHECK_EQUAL(1, bandwidth.Period());
}

// The CPUs a bandwidth needs: quota / period rounded up, exactly.
BOOST_AUTO_TEST_CASE(cpu_bandwidth_cpus_needed_rounds_up)
{
    using namespace bsv::detail;

    constexpr int64_t maxQuota{std::numeric_limits<int64_t>::max()};
    constexpr int64_t twoTo60{1'152'921'504'606'846'976};
    const std::vector<std::pair<tl::expected<CpuBandwidth, CgroupError>, uint64_t>> cases{
        {CpuBandwidth::Make(400'000, DEFAULT_PERIOD), 4},
        // Rounded up, not to nearest.
        {CpuBandwidth::Make(250'000, DEFAULT_PERIOD), 3},
        {CpuBandwidth::Make(220'000, DEFAULT_PERIOD), 3},
        {CpuBandwidth::Make(200'001, DEFAULT_PERIOD), 3},
        {CpuBandwidth::Make(1'550'000, DEFAULT_PERIOD), 16},
        {CpuBandwidth::Make(50'000, DEFAULT_PERIOD), 1},
        {CpuBandwidth::Make(0, DEFAULT_PERIOD), 0},
        {CpuBandwidth::Make(maxQuota, 1), maxQuota},
        {CpuBandwidth::Make(1, maxQuota), 1},
        // Exact where a double is not: 3 * 2^60 + 1 rounds to 3 * 2^60 as one,
        // so Go, which divides doubles, gets 3.
        {CpuBandwidth::Make((3 * twoTo60) + 1, twoTo60), 4},
    };
    for(const auto& [bandwidth, expected] : cases)
    {
        BOOST_TEST_CONTEXT("bandwidth " << bandwidth)
        {
            BOOST_CHECK_EQUAL(expected, bandwidth.value().CpusNeeded());
        }
    }

    // A precondition broken fails loudly.
    BOOST_CHECK_THROW(std::ignore = CpuBandwidth::Make(Unlimited{}, DEFAULT_PERIOD)
                                        .value()
                                        .CpusNeeded(),
                      std::bad_variant_access);
}

// ReadCpuBandwidth reads cpu.max from a cgroup v2 directory. A missing file is
// no cgroup, as in the root cgroup or one without the cpu controller; one of
// VALUE_FILE_SIZE_LIMIT bytes or more is MalformedFile; one that does not open
// or read is SyscallFailed.
BOOST_AUTO_TEST_CASE(read_cpu_bandwidth_reads_cpu_max,
                     *boost::unit_test::label("integration"))
{
    using namespace bsv::detail;
    using namespace bsv::test;
    using enum CgroupError;

    const TempDirectory root;
    const CgroupDir dir{CgroupVersion::V2, root.Path()};
    WriteFile(root.Path() / "cpu.max",
              PaddedTo("150000 100000\n", VALUE_FILE_SIZE_LIMIT - 1));
    BOOST_CHECK_EQUAL(CpuBandwidth::Make(150'000, DEFAULT_PERIOD), ReadCpuBandwidth(dir));
    WriteFile(root.Path() / "cpu.max",
              PaddedTo("150000 100000\n", VALUE_FILE_SIZE_LIMIT));
    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile}, ReadCpuBandwidth(dir));
    std::filesystem::remove(root.Path() / "cpu.max");
    BOOST_CHECK_EQUAL(tl::unexpected{NoCgroup}, ReadCpuBandwidth(dir));
    std::filesystem::create_directory(root.Path() / "cpu.max");
    BOOST_CHECK_EQUAL(tl::unexpected{SyscallFailed}, ReadCpuBandwidth(dir));
}

// ReadCpuBandwidth reads cpu.cfs_quota_us and cpu.cfs_period_us from a cgroup
// v1 directory. Each file has the size limit, and its errors are its own. Go
// returns no limit without reading the period, and reports a v1 file that does
// not read as MalformedFile.
BOOST_AUTO_TEST_CASE(read_cpu_bandwidth_reads_the_v1_files,
                     *boost::unit_test::label("integration"))
{
    using namespace bsv::detail;
    using enum CgroupError;

    const std::string fullQuota{PaddedTo("50000\n", VALUE_FILE_SIZE_LIMIT)};
    const std::string fullPeriod{PaddedTo("100000\n", VALUE_FILE_SIZE_LIMIT)};
    const std::vector<std::pair<Tree, tl::expected<CpuBandwidth, CgroupError>>> cases{
        {{{"cpu.cfs_quota_us", "50000\n"}, {"cpu.cfs_period_us", "100000\n"}},
         CpuBandwidth::Make(50'000, DEFAULT_PERIOD)},
        {{{"cpu.cfs_quota_us", fullQuota}, {"cpu.cfs_period_us", "100000\n"}},
         tl::unexpected{MalformedFile}},
        {{{"cpu.cfs_quota_us", "50000\n"}, {"cpu.cfs_period_us", fullPeriod}},
         tl::unexpected{MalformedFile}},
        // Read errors come before parse errors, the quota's first.
        {{{"cpu.cfs_period_us", "garbage\n"}}, tl::unexpected{NoCgroup}},
        {{{"cpu.cfs_quota_us", "garbage\n"}}, tl::unexpected{NoCgroup}},
        {{{"cpu.cfs_period_us", std::nullopt}}, tl::unexpected{NoCgroup}},
        // The period is read even next to a quota that is no limit.
        {{{"cpu.cfs_quota_us", "-1\n"}}, tl::unexpected{NoCgroup}},
    };
    for(const auto& [tree, expected] : cases)
    {
        const bsv::test::TempDirectory root;
        WriteTree(root.Path(), tree);
        BOOST_TEST_CONTEXT("tree " << Paths(tree))
        {
            BOOST_CHECK_EQUAL(expected,
                              ReadCpuBandwidth(
                                  CgroupDir{CgroupVersion::V1, root.Path()}));
        }
    }
}

// A cgroup's path starts with '/'.
BOOST_AUTO_TEST_CASE(cpu_cgroup_rejects_a_path_not_starting_with_a_slash)
{
    using namespace bsv::detail;
    using enum CgroupError;

    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile},
                      CpuCgroup::Make(CgroupVersion::V2, ""));
    BOOST_CHECK_EQUAL(tl::unexpected{MalformedFile},
                      CpuCgroup::Make(CgroupVersion::V1, "ctr"));
    BOOST_CHECK(CpuCgroup::Make(CgroupVersion::V2, "/").has_value());
    const CpuCgroup cgroup{CpuCgroup::Make(CgroupVersion::V1, "/ctr").value()};
    BOOST_CHECK(cgroup.Version() == CgroupVersion::V1);
    BOOST_CHECK_EQUAL("/ctr", cgroup.Path());
}

// Each proc/self/cgroup and the cgroup ParseCpuCgroup finds in it.
BOOST_AUTO_TEST_CASE(parse_cpu_cgroup)
{
    using namespace bsv::detail;
    using enum CgroupError;

    const std::vector<std::pair<std::string, tl::expected<CpuCgroup, CgroupError>>> cases{
        {"0::/\n", CpuCgroup::Make(CgroupVersion::V2, "/")},
        {"0::/system.slice/node.service\n",
         CpuCgroup::Make(CgroupVersion::V2, "/system.slice/node.service")},
        // Outside the cgroup namespace: ParseCpuCgroupDir rejects it, not this.
        {"0::/../../other\n", CpuCgroup::Make(CgroupVersion::V2, "/../../other")},
        {"12:memory:/kubepods/burstable/pod1/ctr\n11:cpu,cpuacct:/kubepods/burstable/"
         "pod1/ctr\n"
         "10:cpuset:/kubepods/burstable/pod1/ctr\n0::/\n",
         CpuCgroup::Make(CgroupVersion::V1, "/kubepods/burstable/pod1/ctr")},
        // Only a controller list containing exactly "cpu" counts.
        {"10:cpuset:/kubepods/pod1/ctr\n11:cpu,cpuacct:/kubepods/pod1/ctr\n0::/\n",
         CpuCgroup::Make(CgroupVersion::V1, "/kubepods/pod1/ctr")},
        {"10:cpuacct:/a\n0::/b\n", CpuCgroup::Make(CgroupVersion::V2, "/b")},
        // The v1 cpu hierarchy wins over v2 wherever it is listed, and the
        // first one counts.
        {"0::/b\n11:cpu,cpuacct:/a\n", CpuCgroup::Make(CgroupVersion::V1, "/a")},
        {"11:cpu,cpuacct:/a\n12:cpu:/b\n", CpuCgroup::Make(CgroupVersion::V1, "/a")},
        // v2 is hierarchy 0 exactly; other hierarchies without cpu don't count,
        // and neither does an empty file.
        {"5::/x\n", tl::unexpected{NoCgroup}},
        {"10:cpuacct:/a\n", tl::unexpected{NoCgroup}},
        {"0123:cpuacct:/a\n", tl::unexpected{NoCgroup}},
        {"", tl::unexpected{NoCgroup}},
        // Malformed: a line without two colons, a blank line, a path not
        // starting with '/'. Each rejects the file even when a valid line
        // follows.
        {"not a cgroup line\n", tl::unexpected{MalformedFile}},
        {"/a/b\n0::/\n", tl::unexpected{MalformedFile}},
        {"0:cpu\n0::/\n", tl::unexpected{MalformedFile}},
        {"\n0::/\n", tl::unexpected{MalformedFile}},
        {"0::relative\n", tl::unexpected{MalformedFile}},
        {"0::relative\n0::/\n", tl::unexpected{MalformedFile}},
        // A last line without its newline is incomplete, whatever it holds,
        // unless the v1 cpu hierarchy comes first.
        {"0::/", tl::unexpected{IncompleteLine}},
        {"0:cpu", tl::unexpected{IncompleteLine}},
        {"11:cpu:/a\n0::/", CpuCgroup::Make(CgroupVersion::V1, "/a")},
    };
    for(const auto& [cgroupFile, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cgroup file '" << cgroupFile << "'")
        {
            BOOST_CHECK_EQUAL(expected, ParseCpuCgroup(cgroupFile));
        }
    }
}

// Each proc/self/mountinfo and cgroup, and the directory ParseCpuCgroupDir
// builds from them.
BOOST_AUTO_TEST_CASE(parse_cpu_cgroup_dir)
{
    using namespace bsv::detail;
    using enum CgroupError;

    const auto v2Root{CpuCgroup::Make(CgroupVersion::V2, "/")};
    const std::vector<std::tuple<std::string,
                                 tl::expected<CpuCgroup, CgroupError>,
                                 tl::expected<std::string, CgroupError>>>
        cases{
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}), v2Root, "/sys/fs/cgroup"},
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V2, "/system.slice/node.service"),
             "/sys/fs/cgroup/system.slice/node.service"},
            // A space in the mount point is escaped as \040.
            {Concat({ROOT_MOUNT,
                     "30 23 0:26 / /sys/fs/cgroup\\040unified "
                     "rw,nosuid,nodev,noexec,relatime shared:4 - cgroup2 cgroup2 rw\n"}),
             v2Root,
             "/sys/fs/cgroup unified"},
            // A cgroup shown as /../.. lies outside the mount this namespace sees.
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V2, "/../../other"),
             tl::unexpected{NoCgroup}},
            // A mount rooted at /kubepods/podA does not contain /kubepods/podAB/ctr;
            // the next mount, rooted at /, does.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /kubepods/podA /sys/fs/cgroup-a rw,relatime shared:5 - "
                     "cgroup2 cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V2, "/kubepods/podAB/ctr"),
             "/sys/fs/cgroup/kubepods/podAB/ctr"},
            // A mount rooted at the cgroup itself.
            {Concat({ROOT_MOUNT, V1_MOUNTS}),
             CpuCgroup::Make(CgroupVersion::V1, "/kubepods/burstable/pod1/ctr"),
             "/sys/fs/cgroup/cpu,cpuacct"},
            // v1: only a cgroup mount whose super options contain "cpu" counts.
            {Concat({ROOT_MOUNT,
                     "42 30 0:37 /kubepods/pod1/ctr /sys/fs/cgroup/cpuset "
                     "ro,nosuid,nodev,noexec,relatime master:18 - cgroup cgroup "
                     "rw,cpuset\n"
                     "41 30 0:36 /kubepods/pod1/ctr /sys/fs/cgroup/cpu,cpuacct "
                     "ro,nosuid,nodev,noexec,relatime master:17 - cgroup "
                     "cgroup rw,cpu,cpuacct\n"}),
             CpuCgroup::Make(CgroupVersion::V1, "/kubepods/pod1/ctr"),
             "/sys/fs/cgroup/cpu,cpuacct"},
            {Concat({ROOT_MOUNT, CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V1, "/"),
             tl::unexpected{NoCgroup}},
            // A line that does not split rejects the file even when a valid
            // cgroup2 mount follows; so does a bad escape where one is unescaped:
            // in the selected mount point, or the root of a cgroup2 mount. Go's
            // parser indexes out of range on a stray backslash; the kernel writes
            // '\' as \134.
            {Concat({ROOT_MOUNT, "31 23 0:27 / /mnt/broken rw\n", CGROUP2_MOUNT}),
             v2Root,
             tl::unexpected{MalformedFile}},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /sys/fs/cgroup\\09x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             tl::unexpected{InvalidEscape}},
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /x\\777 /sys/fs/cgroup-x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             tl::unexpected{InvalidEscape}},
            // A mount whose root does not start with '/', cgroup or not, rejects
            // the file; an nsfs bind mount is enough, with or without a cgroup2
            // mount after it.
            {Concat({ROOT_MOUNT,
                     "41 25 0:4 net:[4026532296] /run/netns/ns1 rw shared:12 - nsfs nsfs "
                     "rw\n",
                     CGROUP2_MOUNT}),
             v2Root,
             tl::unexpected{MalformedFile}},
            {Concat({ROOT_MOUNT,
                     "41 25 0:4 net:[4026532296] /run/netns/ns1 rw shared:12 - nsfs nsfs "
                     "rw\n"}),
             v2Root,
             tl::unexpected{MalformedFile}},
            // Only the selected mount's mount point is unescaped: a bad escape on
            // a cgroup2 mount whose root does not contain the cgroup is not read.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /elsewhere /mnt/bad\\09x rw,relatime shared:5 - cgroup2 "
                     "cgroup2 rw\n",
                     CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V2, "/kubepods/ctr"),
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
            {std::string(ROOT_MOUNT), v2Root, tl::unexpected{NoCgroup}},
            // A V2 cgroup with v1 cgroup mounts listed first (a hybrid host).
            {Concat({ROOT_MOUNT, V1_CPU_MOUNT, CGROUP2_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V2, "/x"),
             "/sys/fs/cgroup/x"},
            // A v1 cgroup mount needs its source; without one the file is
            // malformed, even when the mount is not the cpu one.
            {Concat({ROOT_MOUNT,
                     "40 30 0:35 / /sys/fs/cgroup/z rw - cgroup\n",
                     V1_CPU_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V1, "/x"),
             tl::unexpected{MalformedFile}},
            {Concat({ROOT_MOUNT,
                     "40 30 0:35 / /sys/fs/cgroup/z rw - cgroup cgroup\n",
                     V1_CPU_MOUNT}),
             CpuCgroup::Make(CgroupVersion::V1, "/x"),
             tl::unexpected{MalformedFile}},
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
             CpuCgroup::Make(CgroupVersion::V2, "/x/../../y"),
             "/m2/x/../../y"},
            // /.. is matched on a path boundary: a directory named ..foo is inside.
            {Concat({ROOT_MOUNT, "31 23 0:27 /x /m1 rw - cgroup2 cgroup2 rw\n"}),
             CpuCgroup::Make(CgroupVersion::V2, "/x/..foo"),
             "/m1/..foo"},
            // The first matching mount wins.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 / /m1 rw - cgroup2 cgroup2 rw\n32 23 0:28 / /m2 rw - "
                     "cgroup2 cgroup2 rw\n"}),
             CpuCgroup::Make(CgroupVersion::V2, "/c"),
             "/m1/c"},
            // A root matches as a prefix of the cgroup, not by its length alone.
            {Concat({ROOT_MOUNT,
                     "31 23 0:27 /aaa /m1 rw - cgroup2 cgroup2 rw\n32 23 0:28 / /m2 rw - "
                     "cgroup2 cgroup2 rw\n"}),
             CpuCgroup::Make(CgroupVersion::V2, "/bbb/ctr"),
             "/m2/bbb/ctr"},
            // An escaped root that contains the cgroup: \134 is a backslash.
            {Concat({ROOT_MOUNT, "31 23 0:27 /a\\134b /m rw - cgroup2 cgroup2 rw\n"}),
             CpuCgroup::Make(CgroupVersion::V2, "/a\\b/ctr"),
             "/m/ctr"},
    };
    for(const auto& [mountinfo, cgroup, expected] : cases)
    {
        BOOST_TEST_CONTEXT(
            "cgroup " << cgroup.value() << ", mountinfo '" << mountinfo << "'")
        {
            const auto expectedDir{expected.transform(
                [&cgroup](const std::string& path)
                { return CgroupDir{cgroup.value().Version(), path}; })};
            BOOST_CHECK_EQUAL(expectedDir, ParseCpuCgroupDir(mountinfo, cgroup.value()));
        }
    }
}

// Each cpu.max and the bandwidth ParseCpuMax reads from it.
BOOST_AUTO_TEST_CASE(parse_cpu_max)
{
    using namespace bsv::detail;
    using enum CgroupError;
    using namespace std::string_literals;
    constexpr int64_t maxQuota{std::numeric_limits<int64_t>::max()};
    const std::vector<std::pair<std::string, tl::expected<CpuBandwidth, CgroupError>>>
        cases{
            {"250000 100000\n", CpuBandwidth::Make(250'000, DEFAULT_PERIOD)},
            // "max" is Unlimited. The period and the newline are still checked,
            // and a file without the space is malformed.
            {"max 100000\n", CpuBandwidth::Make(Unlimited{}, DEFAULT_PERIOD)},
            {"max 100000", tl::unexpected{MalformedFile}},
            {"max\n", tl::unexpected{MalformedFile}},
            {"two and a half\n", tl::unexpected{MalformedFile}},
            // A zero quota reads as it is. A negative one, and a period of zero
            // or less, are malformed; the kernel writes neither. Go accepts
            // both, and divides by the period.
            {"0 100000\n", CpuBandwidth::Make(0, DEFAULT_PERIOD)},
            {"-1 100000\n", tl::unexpected{MalformedFile}},
            {"250000 0\n", tl::unexpected{MalformedFile}},
            {"250000 -100000\n", tl::unexpected{MalformedFile}},
            {"max 0\n", tl::unexpected{MalformedFile}},
            // The period ends at its newline; anything after it is not parsed.
            // ReadCpuBandwidth applies VALUE_FILE_SIZE_LIMIT (see
            // read_cpu_bandwidth_reads_cpu_max).
            {"250000 100000\n00000000000000000000000000000000000000000000000000\n",
             CpuBandwidth::Make(250'000, DEFAULT_PERIOD)},
            {"250000 100000", tl::unexpected{MalformedFile}},
            {"250000 100000\r\n", tl::unexpected{MalformedFile}},
            {"250000  100000\n", tl::unexpected{MalformedFile}},
            {"+250000 +100000\n", CpuBandwidth::Make(250'000, DEFAULT_PERIOD)},
            // The quota is an int64_t: its maximum parses, one above it does not.
            {std::to_string(maxQuota) + " 100000\n",
             CpuBandwidth::Make(maxQuota, DEFAULT_PERIOD)},
            {"9223372036854775808 100000\n", tl::unexpected{MalformedFile}},
            // A NUL inside a number makes it malformed, not a shorter number.
            {"250000\0 100000\n"s, tl::unexpected{MalformedFile}},
            {"", tl::unexpected{MalformedFile}},
    };
    for(const auto& [cpuMax, expected] : cases)
    {
        BOOST_TEST_CONTEXT("cpu.max '" << cpuMax << "'")
        {
            BOOST_CHECK_EQUAL(expected, ParseCpuMax(cpuMax));
        }
    }
}

// Each cpu.cfs_quota_us and cpu.cfs_period_us, and the bandwidth
// ParseCfsBandwidth reads from them.
BOOST_AUTO_TEST_CASE(parse_cfs_bandwidth)
{
    using namespace bsv::detail;
    using enum CgroupError;

    const std::vector<
        std::tuple<std::string, std::string, tl::expected<CpuBandwidth, CgroupError>>>
        cases{
            {"200000\n", "100000\n", CpuBandwidth::Make(200'000, DEFAULT_PERIOD)},
            // A negative quota, usually -1, is Unlimited. The period is still
            // checked.
            {"-1\n", "100000\n", CpuBandwidth::Make(Unlimited{}, DEFAULT_PERIOD)},
            {"-5\n", "100000\n", CpuBandwidth::Make(Unlimited{}, DEFAULT_PERIOD)},
            {"-1\n", "garbage", tl::unexpected{MalformedFile}},
            {"-1\n", "0\n", tl::unexpected{MalformedFile}},
            {"0\n", "100000\n", CpuBandwidth::Make(0, DEFAULT_PERIOD)},
            // A period of zero or less is malformed. Go accepts it, and
            // divides by it.
            {"200000\n", "0\n", tl::unexpected{MalformedFile}},
            {"200000\n", "-100000\n", tl::unexpected{MalformedFile}},
            {"200000", "100000\n", tl::unexpected{MalformedFile}},
            {"-1", "100000\n", tl::unexpected{MalformedFile}},
            {"200000\n", "100000", tl::unexpected{MalformedFile}},
            {"200000\r\n", "100000\n", tl::unexpected{MalformedFile}},
            {"max\n", "100000\n", tl::unexpected{MalformedFile}},
            // A quota below int64_t's range is malformed, not Unlimited.
            {"-9223372036854775809\n", "100000\n", tl::unexpected{MalformedFile}},
    };
    for(const auto& [quota, period, expected] : cases)
    {
        BOOST_TEST_CONTEXT("quota '" << quota << "', period '" << period << "'")
        {
            BOOST_CHECK_EQUAL(expected, ParseCfsBandwidth(quota, period));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
