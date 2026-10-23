#include <cstdint>
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

// A malformed mountinfo line (no " - " separator, or too few fields after
// it), a mount root not starting with /, or a bad \ooo escape in a mount
// point makes the file unreadable even when a valid cgroup2 mount follows,
// as in Go. Where the malformed line would otherwise resolve to a cgroup
// directory, that directory holds a decoy limit.
BOOST_AUTO_TEST_CASE(malformed_mountinfo_gives_no_limit)
{
    for (const char* name : { "malformed_mount_line", "malformed_mount_root", "malformed_mount_tail", "malformed_mount_escape" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
}

BOOST_AUTO_TEST_CASE(unreadable_cgroup_files_give_no_limit)
{
    for (const char* name : { "garbage", "garbage_cgroup", "missing", "missing_cpu_max", "no_such_fixture" })
        BOOST_CHECK_MESSAGE(!FixtureCpuLimit(name).has_value(), name);
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

// EffectiveCpuCount respects the calling thread's affinity mask, which
// hardware_concurrency() ignores. Pin only the calling thread; any created
// process-lifetime pool is idle during this unit test.
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
    struct RestoreAffinity {
        const cpu_set_t& mask;
        ~RestoreAffinity() { sched_setaffinity(0, sizeof(mask), &mask); }
    } restore{ original };
    BOOST_REQUIRE_EQUAL(sched_setaffinity(0, sizeof(one), &one), 0);
    BOOST_CHECK_EQUAL(bsv::detail::EffectiveCpuCount(FixtureRoot("v2_max")), 1u);
}
#endif

// cgroup integers are whole-string base 10: anything after the digits, and
// any surrounding whitespace, rejects the value.
BOOST_AUTO_TEST_CASE(cgroup_integers_parse_the_whole_string)
{
    using bsv::detail::ParseCgroupInt;
    BOOST_CHECK(ParseCgroupInt("100000") == std::optional<int64_t>(100000));
    BOOST_CHECK(ParseCgroupInt("0") == std::optional<int64_t>(0));
    BOOST_CHECK(ParseCgroupInt("-1") == std::optional<int64_t>(-1));
    BOOST_CHECK(ParseCgroupInt("9223372036854775807") == std::optional<int64_t>(INT64_MAX));
    BOOST_CHECK(ParseCgroupInt("-9223372036854775808") == std::optional<int64_t>(INT64_MIN));
    for (const char* text : { "250000x", "100000 ", "100000\n", "1 2", "12.5", "0x10" })
        BOOST_CHECK_MESSAGE(!ParseCgroupInt(text), "trailing characters accepted: '" << text << "'");
    for (const char* text : { "", " 5", "\t5", "+5", "-", "x1", "9223372036854775808", "-9223372036854775809" })
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
    BOOST_CHECK_EQUAL(LimitCpuCount(4, 8.0), 4u);
    BOOST_CHECK_EQUAL(LimitCpuCount(1, 0.5), 1u);
}

BOOST_AUTO_TEST_SUITE_END()
