#pragma once

// Non-installed units shared by txvalidator.cpp and the internal unit suite.
//
// Parallel input verification: how a job is admitted, published and drained.
// txvalidator.cpp's adapter (implVerifyScript) owns one InputJob per large
// transaction; InputVerifyPool is the process-wide set of helpers. The rules
// below are what the code relies on; the tests that hold each one are listed
// in test/core/test_txvalidator.cpp above the parallel_input_verification suite.
//
// 1. Claims. Every participant (the caller, helpers that join) takes the next
//    index from the job's one atomic cursor, so claimed indices form a prefix
//    [0, next). No index above the lowest recorded failure is claimed
//    (InputJob::Claim, UnclaimedInputs), and an input already running above
//    it is cancelled at its next interpreter poll (InputCancellationSource).
// 2. Verdict. The caller decides only after every joined participant has
//    left. A failure at index f is the result only if no input below f was
//    abandoned (lowestFailure_ < lowestCancelled_); success needs all n inputs
//    claimed and none abandoned. Otherwise the job has no verdict: the caller
//    returns CANCELLED, rethrows its token's exception, or verifies serially
//    if its own setup failed. A helper whose allocation fails abandons its
//    input rather than recording bad_alloc as the result; the caller, unless
//    cancelled, then verifies serially from the lowest input not known to
//    pass (SerialResumeIndex). A completed verdict is kept whatever
//    cancellation or token exception arrives afterwards (CompletedResult).
// 3. Drain. The caller never returns, normally or by exception, while a
//    participant is still joined: JobCaller closes the job, waits for joined
//    helpers, then withdraws it from the queue. TryJoin and close share the
//    job mutex, so no helper joins a closed job. The verify callback borrows
//    the transaction, its precomputed hashes, the settings and the caller's
//    token from the caller's stack; this is what keeps them alive.
// 4. Token. Only the calling thread reads the caller's token: before each of
//    its claims, at interpreter polls of its own inputs, and every 10 ms while
//    waiting for joined helpers. It publishes what it sees (cancellation or a
//    token exception) to callerCancelled_, which is all a helper reads.
//    Cleanup on unwind waits without reading the token.
// 5. Admission. busyThreads counts each validating caller once (BusyScope)
//    plus each helper on a job. A helper takes a place under the helper limit
//    (helpersOnJobs_) and then a core (busyThreads below cores_) before it
//    joins, and checks both again before every claim: it leaves when more
//    threads than cores are busy or more places are held than the limit
//    allows. A job also holds no more helpers at once than its helper count,
//    which implVerifyScript sets from the input count: NextJob skips a job
//    that holds them and TryJoin refuses one more, however the helper was
//    woken. Callers are never refused, so the count can briefly exceed the
//    cores; it is a cooperative budget for BDK's own threads, not a cap on
//    process CPU.
// 6. Publication. Offer publishes the job (queue, then openJobs_, seq_cst)
//    before it reads the free capacity. Every release of a core, by a caller
//    or a helper, decrements busyThreads (seq_cst) and then calls
//    ReleasedCore, which reads openJobs_ and the helper places (seq_cst) and
//    wakes a helper after passing through the pool mutex. Either the offer
//    sees the freed core or the release sees the job. A helper that took a
//    place but found no core gives the place back and re-reads busyThreads
//    before sleeping, so a release that saw the place as taken is not lost.
//
// Pool lifetime. InputVerifyPool::Get creates the pool on the first large
// transaction while the helper limit is not 0, sized once from
// EffectiveCpuCount(), which reads the process's affinity mask (the main
// thread's, whichever thread calls): cores - 1 detached helper threads, each
// with an INPUT_HELPER_STACK_BYTES (8 MiB) stack on POSIX. Helper threads
// inherit the affinity of the thread that creates the pool, so a pool created
// from a caller pinned to fewer CPUs runs its helpers on those. The helper
// limit caps the helpers working at once, not the threads created: any
// non-zero limit creates the full pool. Only a limit of 0 set before the first
// large transaction means no helper thread is ever started. Fewer than 2 CPUs,
// or no helper thread starting at all (a thread or process limit), is recorded
// once and never retried: the process verifies serially. Only a failure to
// allocate the pool is retried by a later call. Once a helper has started, the
// pool is published and lives for the rest of the process: helpers never exit
// and nothing destroys it. Idle helpers sleep on a condition variable and use
// no CPU. This avoids ordering pool teardown against static destructors and
// against callers still inside BDK on other threads during exit. It also means
// BDK cannot be unloaded (dlclose) once the pool exists.

#include <internal/txvalidator_internal.hpp>
#include <txerror.h>
#include <atomic>
#include <memory>
#include <optional>
#if BDK_PARALLEL_INPUTS
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#endif

namespace bsv::detail {
#if BDK_PARALLEL_INPUTS
inline constexpr size_t MIN_INPUTS_PER_PARTICIPANT = 8;
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): process-wide pool state, atomic by design
extern std::atomic<size_t> busyThreads;
extern thread_local bool threadIsBusy;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
size_t HelperLimit();

// A job claims a contiguous prefix; its owner drains participants before
// inspecting CompletedResult. Fail records an outcome for a claimed input.
class InputJob {
public:
    using VerifyInput = std::function<TxError(size_t, const task::CCancellationToken&)>;
    // At most `helpers` helpers are joined at once; the caller is not one.
    InputJob(size_t n, VerifyInput verify, const task::CCancellationToken& callerToken, size_t helpers);
    struct HelperTerms {
        size_t cores{ 0 };
        std::atomic<size_t>* places{ nullptr };
        bool placeReturned = false;
    };
    std::optional<size_t> Work(HelperTerms* helper = nullptr);
    std::optional<size_t> Claim();
    size_t UnclaimedInputs() const;
    bool HasWork() const;
    [[nodiscard]] size_t Helpers() const;
    // Unclaimed inputs, or 0 while helpers cannot join: an allocation failed
    // or the job already holds its helpers.
    size_t HelperUnclaimedInputs() const;
    bool TryJoin();
    void Leave();
    void CloseAndWait(bool pollCancellation = true);
    std::optional<TxError> CompletedResult();
    // After CompletedResult gave no verdict: the input to continue serially
    // from, or nullopt if the caller's token cancelled the job.
    std::optional<size_t> SerialResumeIndex();
    void Fail(size_t index, const TxError& result, std::exception_ptr exception);
private:
    static bool HelperShouldLeave(HelperTerms& helper);
    const size_t n_;
    const size_t helpers_;
    const VerifyInput verify_;
    const task::CCancellationToken& callerToken_;
    std::atomic<bool> callerCancelled_{ false };  // only the caller polls its token
    std::atomic<bool> helpersUnavailable_{ false };  // a helper's allocation failed
    std::atomic<size_t> next_{ 0 };
    std::atomic<size_t> lowestFailure_;  // n_ while nothing failed; written under mutex_
    size_t lowestCancelled_;  // abandoned claims, under mutex_; never a real failure

    std::mutex mutex_;
    std::condition_variable left_;
    std::atomic<size_t> joined_{ 0 };  // helpers; written under mutex_, read by NextJob without it
    bool closed_{ false };
    TxError failure_{ bsv::TxErrorOk() };
    std::exception_ptr failureException_;
};

// Detached helpers keep a published pool alive for process lifetime. A stack
// instance has no workers until Create starts them, so unit tests can exercise
// the same admission/queue operations without an interpreter or worker thread.
class InputVerifyPool {
public:
    explicit InputVerifyPool(size_t cores);
    static InputVerifyPool* Get();
    static InputVerifyPool* Existing();
    [[nodiscard]] size_t Helpers() const;
    size_t AvailableHelpers(size_t limit) const;
    // Queues the job and wakes up to its helper count of idle helpers.
    void Offer(const std::shared_ptr<InputJob>& job);
#if BDK_TESTING_HOOKS
    void WakeHelpersForTesting(bool all);
#endif
    void Withdraw(const std::shared_ptr<InputJob>& job);
    size_t QueuedJobs();
    // True exactly when a wake was sent; false if capacity/work cannot admit.
    bool ReleasedCore(size_t busyAfter);
    bool TryTakeHelperPlace();
    static bool TryTakeCore(size_t cores);
    // Caller holds the pool mutex when workers are running.
    std::shared_ptr<InputJob> NextJob();
private:
    static InputVerifyPool* Create();
    static bool StartHelper(InputVerifyPool* pool);
#if !defined(_WIN32)
    static void* RunHelper(void* pool);
#endif
    void Run();
    const size_t cores_;
    size_t helpers_{ 0 };
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::shared_ptr<InputJob>> jobs_;
    std::atomic<size_t> openJobs_{ 0 };
    std::atomic<size_t> helpersOnJobs_{ 0 };  // helpers holding a place under HelperLimit()

    static std::atomic<InputVerifyPool*> created_;
    static std::atomic<bool> noPool_;  // set once Create finds fewer than 2 CPUs or starts no helper
};

class BusyScope {
public:
    BusyScope();
    ~BusyScope();
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;
    BusyScope(BusyScope&&) = delete;
    BusyScope& operator=(BusyScope&&) = delete;
private:
    const bool counted_;
};

class JobCaller {
public:
    JobCaller(InputVerifyPool& pool, std::shared_ptr<InputJob> job);
    ~JobCaller();
    void Finish();
    JobCaller(const JobCaller&) = delete;
    JobCaller& operator=(const JobCaller&) = delete;
    JobCaller(JobCaller&&) = delete;
    JobCaller& operator=(JobCaller&&) = delete;
private:
    InputVerifyPool& pool_;
    const std::shared_ptr<InputJob> job_;
    bool drained_{ false };
};
#endif
} // namespace bsv::detail
