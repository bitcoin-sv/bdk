#pragma once

// Non-installed units shared by txvalidator.cpp and the internal unit suite.
#include <internal/txvalidator_internal.hpp>
#include <txerror.h>
#include <atomic>
#include <memory>
#if BDK_PARALLEL_INPUTS
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#endif

namespace bsv::detail {
// Shared process controls read by the transaction adapter. Other pool/test
// state stays in input_verification.cpp; no internal headers are installed.
extern std::atomic<size_t> parallelInputThreshold;
extern std::atomic<bool> inputCountersEnabled;
extern std::atomic<uint64_t> jobVerifiedInputs;
extern std::atomic<bsv::detail::InputHook> inputHook;
extern std::atomic<bsv::detail::ValidationStageHook> validationStageHook;
extern std::atomic<bsv::detail::InputPoolHook> inputPoolHook;

#if BDK_PARALLEL_INPUTS
inline constexpr size_t MIN_INPUTS_PER_PARTICIPANT = 8;
extern std::atomic<size_t> busyThreads;
extern thread_local bool threadIsBusy;
size_t HelperLimit();

// A job claims a contiguous prefix; its owner drains participants before
// inspecting CompletedResult. Fail records an outcome for a claimed input.
class InputJob {
public:
    using VerifyInput = std::function<TxError(size_t, const task::CCancellationToken&)>;
    InputJob(size_t n, VerifyInput verify, const task::CCancellationToken& callerToken);
    struct HelperTerms {
        size_t cores{ 0 };
        std::atomic<size_t>* places{ nullptr };
        std::atomic<uint64_t>* inputs{ nullptr };
        bool placeReturned = false;
    };
    std::optional<size_t> Work(HelperTerms* helper = nullptr);
    std::optional<size_t> Claim();
    size_t UnclaimedInputs() const;
    bool HasWork() const;
    size_t HelperUnclaimedInputs() const;
    bool TryJoin();
    void Leave();
    void CloseAndWait(bool pollCancellation = true);
    std::optional<TxError> CompletedResult();
    void Fail(size_t index, const TxError& result, std::exception_ptr exception);
private:
    static bool HelperShouldLeave(HelperTerms& helper);
    const size_t n_;
    const VerifyInput verify_;
    const task::CCancellationToken& callerToken_;
    std::atomic<bool> callerCancelled_{ false };  // only the caller polls its token
    std::atomic<bool> helpersUnavailable_{ false };  // setup allocation failed
    std::atomic<size_t> next_{ 0 };
    std::atomic<size_t> lowestFailure_;  // n_ while nothing failed; written under mutex_
    size_t lowestCancelled_;  // abandoned claims, under mutex_; never a real failure

    std::mutex mutex_;
    std::condition_variable left_;
    size_t joined_{ 0 };
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
    size_t Helpers() const;
    size_t AvailableHelpers(size_t limit) const;
    void Offer(const std::shared_ptr<InputJob>& job, size_t maxHelpers);
    void WakeHelpersForTesting(bool all);
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
