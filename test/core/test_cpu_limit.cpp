#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <thread>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_cpu_limit
#else
#define BOOST_TEST_MODULE test_cpu_limitd
#endif
#include <boost/test/unit_test.hpp>
#include <internal/cpu_limit.hpp>
#if defined(__linux__)
#include <sched.h>
#endif

// The value of an optional the test requires to be set. clang-tidy's
// optional-access check does not see BOOST_REQUIRE as a guard; the fallback
// is never returned because the failed REQUIRE ends the test case.
template <typename T>
static T Present(const std::optional<T>& value)
{
    BOOST_REQUIRE(value.has_value());
    return value.value_or(T{});
}

// The helper pool sizes itself from the CPUs this process may use. On Linux a
// container's CPU limit comes from its cgroup; these tests read fixture trees
// under test/core/data/cgroup, each laid out as a root with proc/self/cgroup,
// proc/self/mountinfo and the cgroup files, never the live system.
BOOST_AUTO_TEST_SUITE(container_cpu_limit)

static std::optional<double> FixtureCpuLimit(const std::string& name)
{
    return bsv::detail::CgroupCpuLimit(std::string(BDK_CGROUP_FIXTURE_DIR) + "/" + name);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_quota_in_a_container)
{
    const auto limit = FixtureCpuLimit("v2_quota");
    BOOST_CHECK_CLOSE(Present(limit), 2.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_quota_of_a_nested_cgroup)
{
    // The process's own cgroup has the limit; the root's cpu.max says max.
    const auto limit = FixtureCpuLimit("v2_nested");
    BOOST_CHECK_CLOSE(Present(limit), 1.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_max_is_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v2_max").has_value());
}

BOOST_AUTO_TEST_CASE(cgroup_v1_quota_in_a_container)
{
    const auto limit = FixtureCpuLimit("v1_quota");
    BOOST_CHECK_CLOSE(Present(limit), 2.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v1_quota_minus_one_is_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v1_unlimited").has_value());
}

// A v1 cpuset hierarchy listed before cpu,cpuacct: only the hierarchy whose
// controller list contains exactly "cpu" counts (the cpuset mount holds a
// decoy 4-CPU quota).
BOOST_AUTO_TEST_CASE(cgroup_v1_cpu_controller_is_matched_exactly)
{
    const auto limit = FixtureCpuLimit("v1_cpuset_first");
    BOOST_CHECK_CLOSE(Present(limit), 2.0, 1e-9);
}

// mountinfo escapes a space in the mount point as \040.
BOOST_AUTO_TEST_CASE(cgroup_mount_point_is_unescaped)
{
    const auto limit = FixtureCpuLimit("v2_escaped_mount");
    BOOST_CHECK_CLOSE(Present(limit), 1.2, 1e-9);
}

// A cgroup shown as /../.. lies outside the mount this namespace sees; the
// path it would resolve to holds a decoy limit.
BOOST_AUTO_TEST_CASE(cgroup_outside_the_namespace_gives_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v2_outside_namespace").has_value());
}

// A mount rooted at /kubepods/podA does not contain /kubepods/podAB/ctr; the
// next mount, rooted at /, does (the wrong one would read a decoy 0.5).
BOOST_AUTO_TEST_CASE(cgroup_mount_root_matches_whole_path_components)
{
    const auto limit = FixtureCpuLimit("v2_prefix_not_boundary");
    BOOST_CHECK_CLOSE(Present(limit), 3.0, 1e-9);
}

// A malformed mountinfo line (no " - " separator, or no filesystem type
// after it), a blank line, a bad \ooo escape in the selected mount point, or
// an escape above \377 in the root of a cgroup2 mount makes the file
// unreadable even when a valid cgroup2 mount follows, as in Go. Where the
// malformed line would otherwise resolve to a cgroup directory, that
// directory holds a decoy limit.
BOOST_AUTO_TEST_CASE(malformed_mountinfo_gives_no_limit)
{
    for (const char* name : { "malformed_mount_line", "malformed_mount_tail", "malformed_mount_escape", "blank_mountinfo_line",
             "escape_out_of_range" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

// Go rejects mountinfo when the root of any mount, not only a cgroup mount,
// does not start with '/'. An nsfs bind mount (root net:[...]) listed before
// the cgroup2 mount is enough: Go applies no limit, and neither does BDK.
BOOST_AUTO_TEST_CASE(mount_root_not_starting_with_a_slash_gives_no_limit)
{
    for (const char* name : { "malformed_mount_root", "nsfs_mount_before_cgroup2" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

// Like Go, only the selected mount's mount point is unescaped: a bad escape
// in the mount point of a cgroup2 mount whose root does not contain the
// cgroup is never read.
BOOST_AUTO_TEST_CASE(unselected_mount_point_is_not_unescaped)
{
    const auto limit = FixtureCpuLimit("bad_escape_on_unselected_mount");
    BOOST_CHECK_CLOSE(Present(limit), 2.5, 1e-9);
}

// Go reads only the fields it needs: a cgroup2 mount needs its filesystem type
// and the space after it, not the source or super options. The last line of
// mountinfo needs no newline.
BOOST_AUTO_TEST_CASE(mountinfo_fields_are_read_as_go_reads_them)
{
    for (const char* name : { "short_tail_after_separator", "unterminated_mountinfo_line" })
        BOOST_CHECK_CLOSE(Present(FixtureCpuLimit(name)), 2.5, 1e-9);
}

// A blank line in proc/self/cgroup, a last line without its newline, and a
// cpu.max of 64 bytes or more (Go reads at most 63) are malformed, as in Go.
BOOST_AUTO_TEST_CASE(unreadable_cgroup_files_give_no_limit)
{
    for (const char* name : { "garbage", "garbage_cgroup", "missing", "missing_cpu_max", "no_such_fixture", "blank_cgroup_line",
             "unterminated_cgroup_line", "long_cpu_max" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

// Go takes quota / period as it is, so a zero quota is a limit of 0 CPUs,
// which the rounding raises to 2.
BOOST_AUTO_TEST_CASE(zero_quota_is_a_limit)
{
    for (const char* name : { "v2_zero_quota", "v1_zero_quota" })
        BOOST_CHECK_MESSAGE(Present(FixtureCpuLimit(name)) == 0.0, name);
}

// Go divides by a zero period as float64 does, so 250000 / 0 is +Inf. The
// kernel never reports a period below 1 ms.
BOOST_AUTO_TEST_CASE(zero_period_divides_like_go)
{
    const double limit = Present(FixtureCpuLimit("v2_zero_period"));
    BOOST_CHECK(std::isinf(limit) && limit > 0);
}

// Go's default GOMAXPROCS rule (runtime/cgroup_linux.go, the same from go1.25.0 on,
// adjustCgroupGOMAXPROCS): the CPUs available to the process, lowered to the
// cgroup limit rounded up, and never below 2 because of the limit.
// On this machine, whatever its cgroup: at least one CPU, never more than
// hardware_concurrency(). On Linux this reads the live affinity mask and cgroup.
BOOST_AUTO_TEST_CASE(effective_cpu_count_is_within_the_machine)
{
    const size_t effective = bsv::detail::EffectiveCpuCount();
    const size_t hardware = std::thread::hardware_concurrency();
    const auto limit = bsv::detail::CgroupCpuLimit("/");
    BOOST_TEST_MESSAGE("effective=" << effective << " hardware_concurrency=" << hardware
        << " cgroup_limit=" << (limit ? std::to_string(*limit) : std::string("none")));
    BOOST_CHECK_GE(effective, 1u);
    if (hardware > 0)
        BOOST_CHECK_LE(effective, hardware);
}

#if defined(__linux__)
static std::string FixtureRoot(const std::string& name)
{
    return std::string(BDK_CGROUP_FIXTURE_DIR) + "/" + name;
}

// EffectiveCpuCount lowers the count to the cgroup limit under the root it is
// given. A machine with more than 3 CPUs shows it: 2.5 CPUs round up to 3.
BOOST_AUTO_TEST_CASE(effective_cpu_count_applies_the_cgroup_limit)
{
    const size_t unlimited = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max"));
    const size_t limited = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_quota"));
    BOOST_CHECK_EQUAL(limited, bsv::detail::LimitCpuCount(unlimited, 2.5));
    if (unlimited > 3)
        BOOST_CHECK_EQUAL(limited, 3u);
    else
        BOOST_TEST_MESSAGE("only " << unlimited << " CPUs: the 2.5-CPU limit cannot lower the count here");
}

// EffectiveCpuCount respects the process's affinity mask, which
// hardware_concurrency() ignores. On Linux that is the main thread's mask, and
// Boost.Test runs test cases on the main thread, so pinning it here narrows
// the mask EffectiveCpuCount reads.
BOOST_AUTO_TEST_CASE(effective_cpu_count_applies_the_affinity_mask)
{
    cpu_set_t original;
    CPU_ZERO(&original);
    BOOST_REQUIRE_EQUAL(sched_getaffinity(0, sizeof(original), &original), 0);
    int pinned = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE && pinned < 0; ++cpu)
        if (CPU_ISSET(cpu, &original))
            pinned = cpu;
    BOOST_REQUIRE_GE(pinned, 0);
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(pinned, &one);
    class RestoreAffinity {
    public:
        explicit RestoreAffinity(const cpu_set_t& mask) : mask_{ mask } {}
        ~RestoreAffinity() { sched_setaffinity(0, sizeof(mask_), &mask_); }
        RestoreAffinity(const RestoreAffinity&) = delete;
        RestoreAffinity& operator=(const RestoreAffinity&) = delete;
        RestoreAffinity(RestoreAffinity&&) = delete;
        RestoreAffinity& operator=(RestoreAffinity&&) = delete;

    private:
        cpu_set_t mask_;
    };
    const RestoreAffinity restore{ original };
    BOOST_REQUIRE_EQUAL(sched_setaffinity(0, sizeof(one), &one), 0);
    BOOST_CHECK_EQUAL(bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max")), 1u);
}

// A thread pinned to one CPU still sees the process's CPUs: the count does not
// depend on which thread asks.
BOOST_AUTO_TEST_CASE(effective_cpu_count_ignores_the_calling_threads_mask)
{
    const size_t process = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max"));
    int pinResult = -1;
    size_t fromPinnedThread = 0;
    std::thread pinned([&pinResult, &fromPinnedThread] {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (sched_getaffinity(0, sizeof(mask), &mask) != 0)
            return;
        int first = 0;
        while (first < CPU_SETSIZE && !CPU_ISSET(first, &mask))
            ++first;
        CPU_ZERO(&mask);
        CPU_SET(first, &mask);
        pinResult = sched_setaffinity(0, sizeof(mask), &mask);
        fromPinnedThread = bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max"));
    });
    pinned.join();
    BOOST_REQUIRE_EQUAL(pinResult, 0);
    if (process <= 1)
        BOOST_TEST_MESSAGE("only " << process << " CPU: pinning a thread cannot lower the count here");
    BOOST_CHECK_EQUAL(fromPinnedThread, process);
}
#endif

// cgroup integers are whole-string base 10 with an optional sign, as Go's
// strconv.ParseInt(s, 10, 64): anything after the digits, and any surrounding
// whitespace, rejects the value.
BOOST_AUTO_TEST_CASE(cgroup_integers_parse_the_whole_string)
{
    using bsv::detail::ParseCgroupInt;
    BOOST_CHECK(ParseCgroupInt("100000") == std::optional<int64_t>(100000));
    BOOST_CHECK(ParseCgroupInt("0") == std::optional<int64_t>(0));
    BOOST_CHECK(ParseCgroupInt("-1") == std::optional<int64_t>(-1));
    BOOST_CHECK(ParseCgroupInt("9223372036854775807") == std::optional<int64_t>(INT64_MAX));
    BOOST_CHECK(ParseCgroupInt("-9223372036854775808") == std::optional<int64_t>(INT64_MIN));
    BOOST_CHECK(ParseCgroupInt("+5") == std::optional<int64_t>(5));
    BOOST_CHECK(ParseCgroupInt("+9223372036854775807") == std::optional<int64_t>(INT64_MAX));
    for (const char* text : { "250000x", "100000 ", "100000\n", "1 2", "12.5", "0x10" })
        BOOST_CHECK_MESSAGE(!ParseCgroupInt(text), "trailing characters accepted: '" << text << "'");
    for (const char* text : { "", " 5", "\t5", "+", "-", "+-5", "-+5", "++5", "x1", "9223372036854775808", "-9223372036854775809" })
        BOOST_CHECK_MESSAGE(!ParseCgroupInt(text), "accepted: '" << text << "'");
}

BOOST_AUTO_TEST_CASE(cpu_count_rounds_the_limit_like_gomaxprocs)
{
    using bsv::detail::LimitCpuCount;
    BOOST_CHECK_EQUAL(LimitCpuCount(16, std::nullopt), 16u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 4.0), 4u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 2.5), 3u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 1.01), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 0.5), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, 0.0), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, -1.0), 2u);
    // Go: max(ceil(-Inf), 2) is 2. Go's int32 conversion of +Inf and NaN is
    // platform-specific; BDK leaves the count unchanged.
    BOOST_CHECK_EQUAL(LimitCpuCount(16, -std::numeric_limits<double>::infinity()), 2u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, std::numeric_limits<double>::infinity()), 16u);
    BOOST_CHECK_EQUAL(LimitCpuCount(16, std::numeric_limits<double>::quiet_NaN()), 16u);
    BOOST_CHECK_EQUAL(LimitCpuCount(4, 8.0), 4u);
    BOOST_CHECK_EQUAL(LimitCpuCount(1, 0.5), 1u);
}

BOOST_AUTO_TEST_SUITE_END()
