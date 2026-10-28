#include <atomic>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_txvalidator_internal
#else
#define BOOST_TEST_MODULE test_txvalidator_internald
#endif
#include <boost/test/unit_test.hpp>
#include <internal/cpu_limit.hpp>
#include <internal/txvalidator_internal.hpp>
#include <internal/input_verification.hpp>
#include <txvalidator.hpp>

using namespace bsv::detail;

// The value of an optional the test requires to be set. clang-tidy's
// optional-access check does not see BOOST_REQUIRE as a guard; the fallback
// is never returned because the failed REQUIRE ends the test case.
template <typename T>
static T Present(const std::optional<T>& value)
{
    BOOST_REQUIRE(value.has_value());
    return value.value_or(T{});
}

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
    const auto caller = std::make_shared<CountingSource>();
    const auto callerToken = caller->GetToken();
    const std::atomic<size_t> lowest{ 4 };
    std::atomic<bool> cancelled{ false };
    const auto helper = std::make_shared<InputCancellationSource>(lowest, cancelled);
    helper->SetIndex(1);
    BOOST_CHECK(!helper->GetToken().IsCanceled());
    BOOST_CHECK_EQUAL(caller->reads, 0u);
    const auto source = std::make_shared<InputCancellationSource>(lowest, cancelled, &callerToken);
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
    InternalState(const InternalState&) = delete;
    InternalState& operator=(const InternalState&) = delete;
    InternalState(InternalState&&) = delete;
    InternalState& operator=(InternalState&&) = delete;
};
} // namespace

// No limit of its own on the helpers a job takes.
static constexpr size_t ANY_HELPERS = std::numeric_limits<size_t>::max();

static std::shared_ptr<InputJob> ValidJob(size_t count, const task::CCancellationToken& token, size_t helpers = ANY_HELPERS)
{
    return std::make_shared<InputJob>(count, helpers, [](size_t, const auto&) { return bsv::TxErrorOk(); }, token);
}

BOOST_FIXTURE_TEST_SUITE(input_jobs, InternalState)

BOOST_AUTO_TEST_CASE(claims_form_a_prefix_and_success_requires_all_inputs)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(3, token);
    BOOST_CHECK(!job->CompletedResult());
    for (size_t i = 0; i < 3; ++i) {
        const auto claim = job->Claim();
        BOOST_CHECK_EQUAL(Present(claim), i);
    }
    BOOST_CHECK(!job->Claim());
    job->CloseAndWait();
    BOOST_CHECK(bsv::TxErrorIsOk(Present(job->CompletedResult())));
}

BOOST_AUTO_TEST_CASE(lowest_failure_wins_and_higher_cancellation_cannot_replace_it)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(4, token);
    for (size_t i = 0; i < 4; ++i) BOOST_REQUIRE(job->Claim());
    job->Fail(3, bsv::TxErrorScript(SCRIPT_ERR_EVAL_FALSE), nullptr);
    job->Fail(0, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    job->Fail(1, bsv::TxErrorCancelled(), nullptr);
    job->CloseAndWait();
    const TxError result = Present(job->CompletedResult());
    BOOST_CHECK_EQUAL(result.domain, TX_ERR_DOMAIN_SCRIPT);
    BOOST_CHECK_EQUAL(result.code, SCRIPT_ERR_VERIFY);
}

// Once input f has failed no participant claims an index above it: Claim
// returns none, the job has no work left, and a participant's Work stops at
// its next claim. early_failure_abandons_inflight_higher_inputs in
// test_txvalidator covers inputs already running when f fails.
BOOST_AUTO_TEST_CASE(claims_stop_above_the_lowest_failure)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(8, token);
    for (size_t i = 0; i < 3; ++i) BOOST_REQUIRE(job->Claim());
    job->Fail(1, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    BOOST_CHECK(!job->Claim());
    BOOST_CHECK(!job->HasWork());
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 0u);

    std::vector<size_t> verified;
    InputJob failing(8, ANY_HELPERS, [&](size_t index, const auto&) {
        verified.push_back(index);
        return index == 2 ? bsv::TxErrorScript(SCRIPT_ERR_VERIFY) : bsv::TxErrorOk();
    }, token);
    BOOST_CHECK_EQUAL(Present(failing.Work()), 3u);
    BOOST_CHECK(verified == std::vector<size_t>({ 0, 1, 2 }));
    failing.CloseAndWait();
    BOOST_CHECK_EQUAL(Present(failing.CompletedResult()).code, SCRIPT_ERR_VERIFY);
}

BOOST_AUTO_TEST_CASE(lower_cancelled_input_prevents_a_higher_failure_verdict)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(3, token);
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
    const auto job = ValidJob(2, token);
    BOOST_REQUIRE(job->Claim()); BOOST_REQUIRE(job->Claim());
    job->Fail(1, bsv::TxErrorException(), std::make_exception_ptr(std::runtime_error("higher")));
    job->Fail(0, bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
    job->CloseAndWait();
    BOOST_CHECK_EQUAL(Present(job->CompletedResult()).code, SCRIPT_ERR_VERIFY);
    const auto exceptional = ValidJob(1, token);
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
    const auto job = ValidJob(1, token);
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
    const auto source = std::make_shared<CountingSource>();
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
    const auto source = std::make_shared<CountingSource>();
    const auto token = source->GetToken();
    InputJob job(3, ANY_HELPERS, [](size_t, const auto& inputToken) {
        return inputToken.IsCanceled() ? bsv::TxErrorCancelled() : bsv::TxErrorOk();
    }, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    BOOST_CHECK_EQUAL(Present(job.Work(&terms)), 3u);
    BOOST_CHECK_EQUAL(source->reads, 0u);
    InputJob caller(1, ANY_HELPERS, [](size_t, const auto& inputToken) {
        return inputToken.IsCanceled() ? bsv::TxErrorCancelled() : bsv::TxErrorOk();
    }, token);
    BOOST_CHECK_EQUAL(Present(caller.Work()), 1u);
    BOOST_CHECK_GT(source->reads, 0u);
}

BOOST_AUTO_TEST_CASE(participant_reuses_one_source_for_successive_inputs)
{
    static size_t polledIndex = 0;
    struct PollGuard {
        const InputHook previous = SetInputHookForTesting(+[](size_t index, InputEvent event) {
            if (event == InputEvent::Polled) polledIndex = index;
        });
        PollGuard() = default;
        ~PollGuard() { SetInputHookForTesting(previous); }
        PollGuard(const PollGuard&) = delete;
        PollGuard& operator=(const PollGuard&) = delete;
        PollGuard(PollGuard&&) = delete;
        PollGuard& operator=(PollGuard&&) = delete;
    } const guard;
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
    InputJob job(3, ANY_HELPERS, verify, token);
    BOOST_CHECK_EQUAL(Present(job.Work()), 3u);
    BOOST_CHECK_EQUAL(calls, 3u);
    BOOST_CHECK_EQUAL(observed[0], 0u);
    BOOST_CHECK_EQUAL(observed[1], 1u);
    BOOST_CHECK_EQUAL(observed[2], 2u);
    BOOST_CHECK_EQUAL(Present(job.Work()), 0u);
    retained.reset(); calls = 0; observed.fill(0);
    InputJob helper(3, ANY_HELPERS, verify, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    BOOST_CHECK_EQUAL(Present(helper.Work(&terms)), 3u);
    BOOST_CHECK_EQUAL(calls, 3u);
    BOOST_CHECK_EQUAL(observed[0], 0u);
    BOOST_CHECK_EQUAL(observed[1], 1u);
    BOOST_CHECK_EQUAL(observed[2], 2u);
}

BOOST_AUTO_TEST_CASE(helper_keeps_working_at_the_core_budget)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(3, token);
    std::atomic<size_t> places{ 1 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    busyThreads.store(2);
    BOOST_CHECK_EQUAL(Present(job->Work(&terms)), 3u);
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 0u);
    BOOST_CHECK(!terms.placeReturned);
    BOOST_CHECK_EQUAL(places.load(), 1u);
}

BOOST_AUTO_TEST_CASE(excess_helper_leaves_before_claiming)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    const auto job = ValidJob(3, token);
    std::atomic<size_t> places{ 2 };
    InputJob::HelperTerms terms{ .cores = 2, .places = &places };
    busyThreads.store(3);
    BOOST_CHECK_EQUAL(Present(job->Work(&terms)), 0u);
    BOOST_CHECK_EQUAL(job->UnclaimedInputs(), 3u);
    busyThreads.store(2);
    bsv::SetParallelInputHelpers(1);
    BOOST_CHECK_EQUAL(Present(job->Work(&terms)), 0u);
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
    const auto small = ValidJob(1, token), first = ValidJob(4, token), second = ValidJob(4, token);
    pool.Offer(small); pool.Offer(first); pool.Offer(second);
    BOOST_CHECK(pool.NextJob() == first);
    BOOST_REQUIRE(first->Claim());
    BOOST_CHECK(pool.NextJob() == second);
    BOOST_REQUIRE(second->Claim());
    BOOST_CHECK(pool.NextJob() == first);
    pool.Withdraw(first);
    BOOST_CHECK(pool.NextJob() == second);
    BOOST_CHECK_EQUAL(pool.QueuedJobs(), 2u);
}

// NextJob picks the queued job with the most unclaimed inputs, the earlier
// one on a tie: not the largest transaction, and not inputs a failure made
// unnecessary. released_core_recruits_for_the_largest_queued_job in
// test_txvalidator drives the same choice through a real core release.
BOOST_AUTO_TEST_CASE(next_job_takes_the_most_unclaimed_inputs)
{
    struct Queued {
        size_t inputs;
        size_t claimed;
        std::optional<size_t> failed;
    };
    struct Row {
        const char* name;
        std::vector<Queued> jobs;
        size_t expected;
    };
    const std::vector<Row> rows{
        { "equal_remainders_keep_queue_order", { { 16, 1, std::nullopt }, { 18, 3, std::nullopt } }, 0 },
        { "larger_remainder_behind_a_smaller_one", { { 16, 1, std::nullopt }, { 128, 3, std::nullopt } }, 1 },
        { "remainder_not_transaction_size", { { 128, 120, std::nullopt }, { 16, 0, std::nullopt } }, 1 },
        { "failure_ends_a_remainder", { { 4, 0, std::nullopt }, { 8, 3, 1 } }, 0 },
    };
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    for (const Row& row : rows) {
        BOOST_TEST_CONTEXT(row.name)
        {
            InputVerifyPool pool(2);
            std::vector<std::shared_ptr<InputJob>> jobs;
            for (const Queued& queued : row.jobs) {
                const auto job = ValidJob(queued.inputs, token);
                for (size_t i = 0; i < queued.claimed; ++i) BOOST_REQUIRE(job->Claim());
                if (queued.failed)
                    job->Fail(queued.failed.value_or(0), bsv::TxErrorScript(SCRIPT_ERR_VERIFY), nullptr);
                pool.Offer(job);
                jobs.push_back(job);
            }
            BOOST_CHECK(pool.NextJob() == jobs.at(row.expected));
        }
    }
}

// A job holds no more helpers at once than its helper count: TryJoin refuses
// one more, NextJob passes the job over while it holds them, and a helper
// leaving makes room again. A job with a count of 0 is verified by its caller.
BOOST_AUTO_TEST_CASE(job_holds_at_most_its_helper_count)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    InputVerifyPool pool(4);
    const auto capped = ValidJob(32, token, 1), other = ValidJob(8, token);
    pool.Offer(capped); pool.Offer(other);
    BOOST_CHECK(pool.NextJob() == capped);
    BOOST_REQUIRE(capped->TryJoin());
    BOOST_CHECK(!capped->TryJoin());
    BOOST_CHECK_EQUAL(capped->HelperUnclaimedInputs(), 0u);
    BOOST_CHECK(pool.NextJob() == other);
    capped->Leave();
    BOOST_CHECK(pool.NextJob() == capped);
    BOOST_REQUIRE(capped->TryJoin());
    capped->Leave();

    const auto callerOnly = ValidJob(8, token, 0);
    BOOST_CHECK(!callerOnly->TryJoin());
    BOOST_CHECK_EQUAL(callerOnly->HelperUnclaimedInputs(), 0u);
    BOOST_CHECK_EQUAL(Present(callerOnly->Work()), 8u);
    BOOST_CHECK(bsv::TxErrorIsOk(Present(callerOnly->CompletedResult())));
}

BOOST_AUTO_TEST_CASE(finished_front_jobs_are_removed)
{
    const auto source = task::CCancellationSource::Make();
    const auto token = source->GetToken();
    InputVerifyPool pool(2);
    const auto finished = ValidJob(1, token), live = ValidJob(2, token);
    BOOST_REQUIRE(finished->Work());
    pool.Offer(finished); pool.Offer(live);
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
    pool.Offer(ValidJob(2, token));
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
        CreationGuard() = default;
        ~CreationGuard() { SetInputPoolCreationHookForTesting(hook); SetInputPoolCoresForTesting(cores); }
        CreationGuard(const CreationGuard&) = delete;
        CreationGuard& operator=(const CreationGuard&) = delete;
        CreationGuard(CreationGuard&&) = delete;
        CreationGuard& operator=(CreationGuard&&) = delete;
    } const guard;
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
            explicit StartTogether(std::atomic<bool>& flag) : start{ flag } {}
            ~StartTogether() { start.store(true); }
            StartTogether(const StartTogether&) = delete;
            StartTogether& operator=(const StartTogether&) = delete;
            StartTogether(StartTogether&&) = delete;
            StartTogether& operator=(StartTogether&&) = delete;
        } const together{ start };
        for (auto& pool : pools) {
            callers.threads.emplace_back([&start, slot = &pool] {
                while (!start.load()) std::this_thread::yield();
                *slot = InputVerifyPool::Get();
            });
        }
    }
    BOOST_REQUIRE(pools[0]);
    for (const auto* pool : pools) BOOST_CHECK(pool == pools[0]);
    BOOST_CHECK_EQUAL(attempts.load(), 1u);
    BOOST_CHECK_EQUAL(pools[0]->Helpers(), 1u);
}
#endif
