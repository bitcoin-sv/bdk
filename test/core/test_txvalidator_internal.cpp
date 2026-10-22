#include <atomic>
#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_txvalidator_internal
#else
#define BOOST_TEST_MODULE test_txvalidator_internald
#endif
#include <boost/test/unit_test.hpp>
#include <internal/txvalidator_internal.hpp>
#include <internal/input_verification.hpp>
#include <txvalidator.hpp>
#if defined(__linux__)
#include <sched.h>
#endif

using namespace bsv::detail;

BOOST_AUTO_TEST_SUITE(input_cancellation)

// Pin the strict boundary on the actual source: a finished failure at index
// 5 must leave both index 5 and all lower inputs uncancelled. A <= mutation
// is otherwise invisible once that input has already finished verifying.
BOOST_AUTO_TEST_CASE(early_stop_source_preserves_equal_and_lower_indices)
{
    std::atomic<size_t> lowest{ 32 };
    const auto caller = task::CCancellationSource::Make();
    const auto callerToken = caller->GetToken();
    std::atomic<bool> cancelled{ false };
    const auto source = std::make_shared<bsv::detail::InputCancellationSource>(lowest, cancelled, &callerToken);
    const auto token = source->GetToken();
    source->SetIndex(5);
    BOOST_CHECK(!token.IsCanceled());
    lowest = 5;
    BOOST_CHECK(!token.IsCanceled());
    source->SetIndex(4);
    BOOST_CHECK(!token.IsCanceled());
    source->SetIndex(6);
    BOOST_CHECK(token.IsCanceled());
    lowest = 3;
    source->SetIndex(4);
    BOOST_CHECK(token.IsCanceled());
    source->SetIndex(2);
    BOOST_CHECK(!token.IsCanceled());
    caller->Cancel();
    BOOST_CHECK(token.IsCanceled());
}



BOOST_AUTO_TEST_CASE(helper_reads_published_flag_without_calling_caller)
{
    struct CountingSource final : task::CCancellationSource {
        unsigned reads{ 0 };
        bool IsCanceled() override { ++reads; return true; }
    };
    auto caller = std::make_shared<CountingSource>();
    const auto callerToken = caller->GetToken();
    std::atomic<size_t> lowest{ 4 };
    std::atomic<bool> cancelled{ false };
    auto helper = std::make_shared<InputCancellationSource>(lowest, cancelled);
    helper->SetIndex(1);
    BOOST_CHECK(!helper->GetToken().IsCanceled());
    BOOST_CHECK_EQUAL(caller->reads, 0u);
    auto source = std::make_shared<InputCancellationSource>(lowest, cancelled, &callerToken);
    BOOST_CHECK(source->GetToken().IsCanceled());
    BOOST_CHECK_EQUAL(caller->reads, 1u);
    BOOST_CHECK(helper->GetToken().IsCanceled());
    BOOST_CHECK_EQUAL(caller->reads, 1u);
}

BOOST_AUTO_TEST_SUITE_END()

#if BDK_PARALLEL_INPUTS
namespace {
struct InternalState {
    const size_t busy = busyThreads.exchange(0);
    const int64_t limit = bsv::GetParallelInputHelpers();
    InternalState() { bsv::SetParallelInputHelpers(-1); }
    ~InternalState() { busyThreads.store(busy); bsv::SetParallelInputHelpers(limit); }
};

std::shared_ptr<InputJob> ValidJob(size_t count, const task::CCancellationToken& token)
{
    return std::make_shared<InputJob>(count, [](size_t, const auto&) { return bsv::TxErrorOk(); }, token);
}
}

BOOST_FIXTURE_TEST_SUITE(input_jobs, InternalState)

BOOST_AUTO_TEST_CASE(claims_form_a_prefix_and_success_requires_all_inputs)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(3, token);
    BOOST_CHECK(!job->CompletedResult());
    for (size_t i = 0; i < 3; ++i) {
        const auto claim = job->Claim();
        BOOST_REQUIRE(claim);
        BOOST_CHECK_EQUAL(*claim, i);
    }
    BOOST_CHECK(!job->Claim());
    job->CloseAndWait();
    BOOST_REQUIRE(job->CompletedResult());
    BOOST_CHECK(bsv::TxErrorIsOk(*job->CompletedResult()));
}

BOOST_AUTO_TEST_CASE(lowest_failure_wins_and_higher_cancellation_cannot_replace_it)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(4, token);
    for (size_t i = 0; i < 4; ++i) BOOST_REQUIRE(job->Claim());
    job->Fail(3, bsv::TxErrorScript(SCRIPT_ERR_EVAL_FALSE), nullptr);
    job->Fail(0, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    job->Fail(1, bsv::TxErrorCancelled(), nullptr);
    job->CloseAndWait();
    const auto result = job->CompletedResult();
    BOOST_REQUIRE(result);
    BOOST_CHECK_EQUAL(result->domain, TX_ERR_DOMAIN_SCRIPT);
    BOOST_CHECK_EQUAL(result->code, SCRIPT_ERR_VERIFY);
}

BOOST_AUTO_TEST_CASE(lower_cancelled_input_prevents_a_higher_failure_verdict)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(3, token);
    for (size_t i = 0; i < 3; ++i) BOOST_REQUIRE(job->Claim());
    job->Fail(2, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    job->Fail(1, bsv::TxErrorCancelled(), nullptr);
    job->CloseAndWait();
    BOOST_CHECK(!job->CompletedResult());
}

BOOST_AUTO_TEST_CASE(stored_exception_keeps_input_order)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(2, token);
    BOOST_REQUIRE(job->Claim()); BOOST_REQUIRE(job->Claim());
    job->Fail(1, bsv::TxErrorException(), std::make_exception_ptr(std::runtime_error("higher")));
    job->Fail(0, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    job->CloseAndWait();
    BOOST_CHECK_EQUAL(job->CompletedResult()->code, SCRIPT_ERR_VERIFY);
    auto exceptional = ValidJob(1, token);
    BOOST_REQUIRE(exceptional->Claim());
    exceptional->Fail(0, bsv::TxErrorException(), std::make_exception_ptr(std::runtime_error("first")));
    exceptional->CloseAndWait();
    BOOST_CHECK_EXCEPTION(exceptional->CompletedResult(), std::runtime_error,
        [](const auto& error) { return std::string(error.what()) == "first"; });
}

BOOST_AUTO_TEST_CASE(closed_job_rejects_new_participants)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(1, token);
    BOOST_REQUIRE(job->TryJoin());
    job->Leave();
    job->CloseAndWait();
    BOOST_CHECK(!job->TryJoin());
}

BOOST_AUTO_TEST_CASE(close_drains_joined_work_without_reading_token)
{
    struct CountingSource final : task::CCancellationSource {
        std::atomic<unsigned> reads{ 0 };
        bool IsCanceled() override { ++reads; return false; }
    };
    auto source = std::make_shared<CountingSource>();
    const auto token = source->GetToken();
    auto job = ValidJob(1, token);
    BOOST_REQUIRE(job->TryJoin());
    std::atomic<bool> done{ false };
    std::thread closer([&] { job->CloseAndWait(false); done = true; });
    bool closed = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!job->TryJoin()) { closed = true; break; }
        job->Leave();
        std::this_thread::yield();
    }
    const bool waited = !done.load();
    job->Leave();
    closer.join();
    BOOST_CHECK(closed);
    BOOST_CHECK(waited);
    BOOST_CHECK(done);
    BOOST_CHECK_EQUAL(source->reads.load(), 0u);
}

BOOST_AUTO_TEST_CASE(helper_work_never_polls_caller_token)
{
    struct CountingSource final : task::CCancellationSource {
        unsigned reads{ 0 };
        bool IsCanceled() override { ++reads; return false; }
    };
    auto source = std::make_shared<CountingSource>();
    const auto token = source->GetToken();
    InputJob job(3, [](size_t, const auto& inputToken) {
        return inputToken.IsCanceled() ? bsv::TxErrorCancelled() : bsv::TxErrorOk();
    }, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    BOOST_CHECK_EQUAL(job.Work(&terms).value(), 3u);
    BOOST_CHECK_EQUAL(source->reads, 0u);
    InputJob caller(1, [](size_t, const auto& inputToken) {
        return inputToken.IsCanceled() ? bsv::TxErrorCancelled() : bsv::TxErrorOk();
    }, token);
    BOOST_CHECK_EQUAL(caller.Work().value(), 1u);
    BOOST_CHECK_GT(source->reads, 0u);
}

BOOST_AUTO_TEST_CASE(participant_reuses_one_source_for_successive_inputs)
{
    static size_t polledIndex = 0;
    struct PollGuard {
        const InputHook previous = SetInputHookForTesting(+[](size_t index, InputEvent event) {
            if (event == InputEvent::Polled) polledIndex = index;
        });
        ~PollGuard() { SetInputHookForTesting(previous); }
    } guard;
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    std::optional<task::CCancellationToken> retained;
    std::array<size_t, 3> observed{};
    size_t calls = 0;
    const auto verify = [&](size_t, const auto& inputToken) {
        if (!retained) retained.emplace(inputToken);
        // A copy of the first token keeps its actual source alive. That source
        // must follow every later SetIndex; re-emplacing an optional at the
        // same address cannot satisfy this assertion with a different source.
        const bool cancelled = retained->IsCanceled();
        observed.at(calls++) = polledIndex;
        return cancelled ? bsv::TxErrorCancelled() : bsv::TxErrorOk();
    };
    InputJob job(3, verify, token);
    BOOST_CHECK_EQUAL(job.Work().value(), 3u);
    BOOST_CHECK_EQUAL(calls, 3u);
    BOOST_CHECK_EQUAL(observed[0], 0u);
    BOOST_CHECK_EQUAL(observed[1], 1u);
    BOOST_CHECK_EQUAL(observed[2], 2u);
    BOOST_CHECK_EQUAL(job.Work().value(), 0u);
    retained.reset(); calls = 0; observed.fill(0);
    InputJob helper(3, verify, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    BOOST_CHECK_EQUAL(helper.Work(&terms).value(), 3u);
    BOOST_CHECK_EQUAL(calls, 3u);
    BOOST_CHECK_EQUAL(observed[0], 0u);
    BOOST_CHECK_EQUAL(observed[1], 1u);
    BOOST_CHECK_EQUAL(observed[2], 2u);
}

BOOST_AUTO_TEST_CASE(helper_keeps_working_at_the_core_budget)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(3, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    busyThreads.store(2);
    BOOST_CHECK_EQUAL(job->Work(&terms).value(), 3u);
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 0u);
    BOOST_CHECK(!terms.placeReturned);
    BOOST_CHECK_EQUAL(places.load(), 1u);
}

BOOST_AUTO_TEST_CASE(excess_helper_leaves_before_claiming)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    auto job = ValidJob(3, token);
    std::atomic<size_t> places{ 2 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    busyThreads.store(3);
    BOOST_CHECK_EQUAL(job->Work(&terms).value(), 0u);
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 3u);
    busyThreads.store(2);
    bsv::SetParallelInputHelpers(1);
    BOOST_CHECK_EQUAL(job->Work(&terms).value(), 0u);
    BOOST_CHECK(terms.placeReturned);
    BOOST_CHECK_EQUAL(places.load(), 1u);
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 3u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(pool_admission, InternalState)

BOOST_AUTO_TEST_CASE(helper_cap_and_core_budget_each_limit_admission)
{
    InputVerifyPool pool(2);
    bsv::SetParallelInputHelpers(1);
    BOOST_CHECK(pool.TryTakeHelperPlace());
    BOOST_CHECK(!pool.TryTakeHelperPlace());
    bsv::SetParallelInputHelpers(2);
    BOOST_CHECK(pool.TryTakeHelperPlace());
    BOOST_CHECK(!pool.TryTakeHelperPlace());
    BOOST_CHECK(InputVerifyPool::TryTakeCore(2));
    BOOST_CHECK(InputVerifyPool::TryTakeCore(2));
    BOOST_CHECK(!InputVerifyPool::TryTakeCore(2));
    busyThreads.fetch_sub(1);
    BOOST_CHECK(InputVerifyPool::TryTakeCore(2));
}

BOOST_AUTO_TEST_CASE(nested_callers_count_once_and_release_their_core)
{
    BOOST_CHECK_EQUAL(busyThreads.load(), 0u);
    {
        const BusyScope outer;
        BOOST_CHECK_EQUAL(busyThreads.load(), 1u);
        {
            const BusyScope inner;
            BOOST_CHECK_EQUAL(busyThreads.load(), 1u);
        }
        BOOST_CHECK_EQUAL(busyThreads.load(), 1u);
    }
    BOOST_CHECK_EQUAL(busyThreads.load(), 0u);
}

BOOST_AUTO_TEST_CASE(largest_job_wins_and_positive_ties_keep_queue_order)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    InputVerifyPool pool(2);
    auto small = ValidJob(1, token), first = ValidJob(4, token), second = ValidJob(4, token);
    pool.Offer(small, 0); pool.Offer(first, 0); pool.Offer(second, 0);
    BOOST_CHECK(pool.NextJob() == first);
    BOOST_REQUIRE(first->Claim());
    BOOST_CHECK(pool.NextJob() == second);
    BOOST_REQUIRE(second->Claim());
    BOOST_CHECK(pool.NextJob() == first);
    pool.Withdraw(first);
    BOOST_CHECK(pool.NextJob() == second);
    BOOST_CHECK_EQUAL(pool.QueuedJobs(), 2u);
}

BOOST_AUTO_TEST_CASE(finished_front_jobs_are_removed)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    InputVerifyPool pool(2);
    auto finished = ValidJob(1, token), live = ValidJob(2, token);
    BOOST_REQUIRE(finished->Work());
    pool.Offer(finished, 0); pool.Offer(live, 0);
    BOOST_CHECK(pool.NextJob() == live);
    BOOST_CHECK_EQUAL(pool.QueuedJobs(), 1u);
    pool.Withdraw(live);
    BOOST_CHECK(!pool.NextJob());
}

BOOST_AUTO_TEST_CASE(core_release_notifies_only_with_queued_work_and_capacity)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    InputVerifyPool pool(2);
    BOOST_CHECK(!pool.ReleasedCore(0));
    pool.Offer(ValidJob(2, token), 0);
    BOOST_CHECK(!pool.ReleasedCore(2));
    BOOST_CHECK(pool.ReleasedCore(1));
    bsv::SetParallelInputHelpers(1);
    BOOST_REQUIRE(pool.TryTakeHelperPlace());
    BOOST_CHECK(!pool.ReleasedCore(0));
    bsv::SetParallelInputHelpers(2);
    BOOST_CHECK(pool.ReleasedCore(0));
}

BOOST_AUTO_TEST_SUITE_END()

// Skips, rather than passes, a case that needs a pool on a 1-CPU machine.
static boost::test_tools::assertion_result TwoEffectiveCpus(boost::unit_test::test_unit_id)
{
    boost::test_tools::assertion_result result(EffectiveCpuCount() >= 2);
    result.message() << "requires at least two effective CPUs";
    return result;
}

BOOST_AUTO_TEST_CASE(concurrent_first_callers_share_one_pool, *boost::unit_test::precondition(TwoEffectiveCpus))
{
    BOOST_REQUIRE(InputVerifyPool::Existing() == nullptr);
    static std::atomic<unsigned> attempts{ 0 };
    struct CreationGuard {
        const size_t cores = SetInputPoolCoresForTesting(2);
        const InputPoolCreationHook hook = SetInputPoolCreationHookForTesting(+[] { ++attempts; });
        ~CreationGuard() { SetInputPoolCreationHookForTesting(hook); SetInputPoolCoresForTesting(cores); }
    } guard;
    std::array<InputVerifyPool*, 8> pools{};
    std::atomic<bool> start{ false };
    {
        struct JoinedCallers {
            std::vector<std::thread> threads;
            JoinedCallers() = default;
            ~JoinedCallers()
            {
                for (auto& thread : threads)
                    if (thread.joinable()) thread.join();
            }
            JoinedCallers(const JoinedCallers&) = delete;
            JoinedCallers& operator=(const JoinedCallers&) = delete;
            JoinedCallers(JoinedCallers&&) = delete;
            JoinedCallers& operator=(JoinedCallers&&) = delete;
        } callers;
        struct StartTogether {
            std::atomic<bool>& start;
            ~StartTogether() { start.store(true); }
        } together{ start };
        for (size_t i = 0; i < pools.size(); ++i) {
            callers.threads.emplace_back([&, i] {
                while (!start.load()) std::this_thread::yield();
                pools[i] = InputVerifyPool::Get();
            });
        }
    }
    BOOST_REQUIRE(pools[0]);
    for (const auto* pool : pools) BOOST_CHECK(pool == pools[0]);
    BOOST_CHECK_EQUAL(attempts.load(), 1u);
    BOOST_CHECK_EQUAL(pools[0]->Helpers(), 1u);
}
#endif

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
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_quota_of_a_nested_cgroup)
{
    // The process's own cgroup has the limit; the root's cpu.max says max.
    const auto limit = FixtureCpuLimit("v2_nested");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 1.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(cgroup_v2_max_is_no_limit)
{
    BOOST_CHECK(!FixtureCpuLimit("v2_max").has_value());
}

BOOST_AUTO_TEST_CASE(cgroup_v1_quota_in_a_container)
{
    const auto limit = FixtureCpuLimit("v1_quota");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.0, 1e-9);
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
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 2.0, 1e-9);
}

// mountinfo escapes a space in the mount point as \040.
BOOST_AUTO_TEST_CASE(cgroup_mount_point_is_unescaped)
{
    const auto limit = FixtureCpuLimit("v2_escaped_mount");
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 1.2, 1e-9);
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
    BOOST_REQUIRE(limit.has_value());
    BOOST_CHECK_CLOSE(*limit, 3.0, 1e-9);
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
