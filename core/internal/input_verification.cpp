#include <internal/input_verification.hpp>
#include <internal/cpu_limit.hpp>
#include <txvalidator.hpp>
#if BDK_PARALLEL_INPUTS
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#endif


namespace bsv::detail {
// The default threshold, bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD, comes
// from a serial-vs-parallel sweep over input counts (SIGHASH_ALL|FORKID
// P2PKH, about 20-50 us per input): at 16 inputs parallel was 1.56x-1.84x
// faster on ubuntu-22.04 (x86-64, 4 vCPU), ubuntu-22.04-arm (4 cores),
// macos-15 CI (3 vCPU) and an Apple M3 Max; at 2-4 inputs the macos-15 runner
// was slower.
static_assert(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD == 16);
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): process-wide pool state and test hooks, atomic by design
std::atomic<size_t> parallelInputThreshold{ bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD };
// Test-only counters, written only while inputCountersEnabled (off by
// default); participation counters read the flag outside the input loop.
std::atomic<bool> inputCountersEnabled{ false };
static std::atomic<uint64_t> helperVerifiedInputs{ 0 };
std::atomic<uint64_t> jobVerifiedInputs{ 0 };  // caller and helpers, inside jobs
static std::atomic<uint64_t> helperJobParticipations{ 0 };  // helper Work() calls that verified an input
static std::atomic<uint64_t> helperWakesWithoutCore{ 0 };
std::atomic<bsv::detail::InputHook> inputHook{ nullptr };  // tests only
static std::atomic<bsv::detail::InputSourceAllocationHook> inputSourceAllocationHook{ nullptr };  // tests only
std::atomic<bsv::detail::ValidationStageHook> validationStageHook{ nullptr };  // tests only
static std::atomic<bsv::detail::HelperReleasedCoreHook> helperReleasedCoreHook{ nullptr };  // tests only
std::atomic<bsv::detail::InputPoolHook> inputPoolHook{ nullptr };  // tests only
static std::atomic<size_t> inputPoolCoresCap{ 0 };  // 0: no cap (tests only)
static std::atomic<bsv::detail::InputPoolCreationHook> inputPoolCreationHook{ nullptr };
static std::atomic<int64_t> parallelInputHelpers{ -1 };  // SetParallelInputHelpers; < 0: automatic
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)


#if BDK_PARALLEL_INPUTS
// How often a caller waiting for joined helpers polls its token.
static constexpr std::chrono::milliseconds CALLER_CANCELLATION_POLL_INTERVAL{ 10 };

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): process-wide pool state and test hooks, atomic by design
std::atomic<size_t> busyThreads{ 0 };
thread_local bool threadIsBusy{ false };
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// The most helpers that may be on jobs at once, from SetParallelInputHelpers.
size_t HelperLimit()
{
    const int64_t helpers = parallelInputHelpers.load(std::memory_order_relaxed);
    if (helpers < 0)
        return std::numeric_limits<size_t>::max();
    return static_cast<size_t>(std::min<uint64_t>(static_cast<uint64_t>(helpers), std::numeric_limits<size_t>::max()));
}

// One transaction's inputs [0, n), shared by its caller and the helpers that join.
InputJob::InputJob(size_t n, VerifyInput verify, const task::CCancellationToken& callerToken)
    : n_{ n }, verify_{ std::move(verify) }, callerToken_{ callerToken },
      lowestFailure_{ n }, lowestCancelled_{ n } {}

std::optional<size_t> InputJob::Work(HelperTerms* helper)
{
    size_t verified = 0;
    const auto pollHook = inputHook.load(std::memory_order_relaxed);
    std::shared_ptr<bsv::detail::InputCancellationSource> cancellation;
    std::optional<task::CCancellationToken> inputToken;
    if (!HasWork() || (helper != nullptr && HelperShouldLeave(*helper)))
        return 0;
    // Participant setup must not become an input failure or consume an
    // index. A helper can leave; the caller drains, retaining any completed
    // verdict before falling back to serial verification if needed.
    try {
        if (const auto hook = inputSourceAllocationHook.load(std::memory_order_relaxed))
            hook(helper != nullptr);
        cancellation = std::make_shared<bsv::detail::InputCancellationSource>(
            lowestFailure_, callerCancelled_, helper == nullptr ? &callerToken_ : nullptr, pollHook);
        inputToken.emplace(cancellation->GetToken());
    } catch (const std::bad_alloc&) {
        // Do not spend helper CPU retrying allocation on this job. The
        // caller and participants already admitted can finish its inputs.
        if (helper != nullptr)
            helpersUnavailable_.store(true, std::memory_order_relaxed);
        return std::nullopt;
    }
    for (;;) {
        if (helper != nullptr && HelperShouldLeave(*helper))
            return verified;
        // Stop claiming once no necessary input remains. The caller's
        // drain wait separately polls while joined helpers remain.
        if (!HasWork())
            return verified;
        if (helper == nullptr) {
            try {
                if (callerToken_.IsCanceled()) {
                    callerCancelled_.store(true, std::memory_order_relaxed);
                    if (pollHook)
                        pollHook(next_.load(std::memory_order_relaxed), bsv::detail::InputEvent::CallerCancelled);
                }
            } catch (...) {
                callerCancelled_.store(true, std::memory_order_relaxed);
                throw;
            }
        }
        if (callerCancelled_.load(std::memory_order_relaxed))
            return verified;
        const auto claimed = Claim();
        if (!claimed)
            return verified;
        const size_t index = *claimed;
        if (const auto hook = inputHook.load(std::memory_order_relaxed))
            hook(index, bsv::detail::InputEvent::Claimed);
        ++verified;
        try {
            cancellation->SetIndex(index);
            const TxError result = verify_(index, *inputToken);
            if (pollHook)
                pollHook(index, bsv::detail::InputEvent::Verified);
            if (!bsv::TxErrorIsOk(result))
                Fail(index, result, nullptr);
        } catch (...) {
            Fail(index, bsv::TxErrorException(), std::current_exception());
        }
        if (helper != nullptr && helper->inputs != nullptr)
            helper->inputs->fetch_add(1, std::memory_order_relaxed);
    }
}

std::optional<size_t> InputJob::Claim()
{
    const size_t index = next_.fetch_add(1, std::memory_order_relaxed);
    if (index >= n_ || index > lowestFailure_.load(std::memory_order_relaxed))
        return std::nullopt;
    return index;
}

size_t InputJob::UnclaimedInputs() const
{
    const size_t next = next_.load(std::memory_order_relaxed);
    return !callerCancelled_.load(std::memory_order_relaxed)
        && next < n_ && next <= lowestFailure_.load(std::memory_order_relaxed) ? n_ - next : 0;
}

bool InputJob::HasWork() const { return UnclaimedInputs() != 0; }

size_t InputJob::HelperUnclaimedInputs() const
{
    return helpersUnavailable_.load(std::memory_order_relaxed) ? 0 : UnclaimedInputs();
}

bool InputJob::TryJoin()
{
    const std::scoped_lock lock{ mutex_ };
    if (closed_ || helpersUnavailable_.load(std::memory_order_relaxed))
        return false;
    ++joined_;
    return true;
}

void InputJob::Leave()
{
    const std::scoped_lock lock{ mutex_ };
    if (--joined_ == 0 && closed_)
        left_.notify_one();
}

void InputJob::CloseAndWait(bool pollCancellation)
{
    std::unique_lock lock{ mutex_ };
    if (!closed_) {
        closed_ = true;
        if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
            hook(bsv::detail::InputPoolEvent::JobClosed);
    }
    while (joined_ != 0) {
        if (!pollCancellation || callerCancelled_.load(std::memory_order_relaxed)) {
            left_.wait(lock, [this] { return joined_ == 0; });
            break;
        }
        // Only an active caller tail wait has a timer. Idle pool helpers
        // remain parked; an already-drained job never polls here.
        if (left_.wait_for(lock, CALLER_CANCELLATION_POLL_INTERVAL, [this] { return joined_ == 0; }))
            break;
        // A token can invoke caller code. Do not hold the job mutex while
        // polling it, and never invoke it from noexcept unwind cleanup.
        lock.unlock();
        try {
            if (callerToken_.IsCanceled()) {
                callerCancelled_.store(true, std::memory_order_relaxed);
                if (const auto hook = inputHook.load(std::memory_order_relaxed))
                    hook(next_.load(std::memory_order_relaxed), bsv::detail::InputEvent::CallerCancelled);
            }
        } catch (...) {
            callerCancelled_.store(true, std::memory_order_relaxed);
            throw;  // JobCaller drains helpers before the exception escapes
        }
        lock.lock();
    }
}

std::optional<TxError> InputJob::CompletedResult()
{
    const std::scoped_lock lock{ mutex_ };
    // Claims form a contiguous prefix, and every joined participant has
    // drained. A real failure is definitive only if no lower claim was
    // abandoned. Higher cancellations cannot replace that verdict.
    if (lowestFailure_.load(std::memory_order_relaxed) < lowestCancelled_) {
        if (failureException_)
            std::rethrow_exception(failureException_);
        return failure_;
    }
    if (lowestCancelled_ < n_ || next_.load(std::memory_order_relaxed) < n_)
        return std::nullopt;
    return bsv::TxErrorOk();
}

bool InputJob::HelperShouldLeave(HelperTerms& helper)
{
    if (busyThreads.load(std::memory_order_relaxed) > helper.cores)
        return true;
    const size_t limit = HelperLimit();
    size_t onJobs = helper.places->load(std::memory_order_relaxed);
    while (onJobs > limit) {
        if (helper.places->compare_exchange_weak(onJobs, onJobs - 1, std::memory_order_relaxed)) {
            helper.placeReturned = true;
            return true;
        }
    }
    return false;
}

void InputJob::Fail(size_t index, const TxError& result, std::exception_ptr exception)
{
    if (const auto hook = inputHook.load(std::memory_order_relaxed))
        hook(index, bsv::detail::InputEvent::Failed);
    const std::scoped_lock lock{ mutex_ };
    if (result.domain == TX_ERR_DOMAIN_CANCELLED) {
        lowestCancelled_ = std::min(lowestCancelled_, index);
        return;
    }
    if (index >= lowestFailure_.load(std::memory_order_relaxed))
        return;
    lowestFailure_.store(index, std::memory_order_relaxed);
    failure_ = result;
    failureException_ = std::move(exception);
}

InputVerifyPool* InputVerifyPool::Get()
{
    if (auto* pool = Existing())
        return pool;
    if (tooFewCores_.load(std::memory_order_acquire))
        return nullptr;
    static std::mutex creationMutex;
    const std::scoped_lock lock{ creationMutex };
    if (auto* pool = Existing())
        return pool;
    if (tooFewCores_.load(std::memory_order_relaxed))
        return nullptr;
    try {
        return Create();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

InputVerifyPool* InputVerifyPool::Existing() { return created_.load(std::memory_order_acquire); }

size_t InputVerifyPool::Helpers() const { return helpers_; }

size_t InputVerifyPool::AvailableHelpers(size_t limit) const
{
    const size_t busy = busyThreads.load(std::memory_order_seq_cst);
    const size_t onJobs = helpersOnJobs_.load(std::memory_order_relaxed);
    if (busy >= cores_ || onJobs >= limit)
        return 0;
    return std::min({ helpers_, cores_ - busy, limit - onJobs });
}

void InputVerifyPool::Offer(const std::shared_ptr<InputJob>& job, size_t maxHelpers)
{
    {
        const std::scoped_lock lock{ mutex_ };
        jobs_.push_back(job);
        openJobs_.fetch_add(1, std::memory_order_seq_cst);
    }
    // Publish before the decisive capacity read. A release that saw no
    // open job is visible here; a later release sees the published job
    // and notifies through ReleasedCore. Do not wake a genuinely full pool.
    const size_t helpers = std::min(AvailableHelpers(HelperLimit()), maxHelpers);
    for (size_t i = 0; i < helpers; ++i)
        wake_.notify_one();
}

void InputVerifyPool::WakeHelpersForTesting(bool all)
{
    const std::scoped_lock lock{ mutex_ };
    if (all) wake_.notify_all();
    else wake_.notify_one();
}

void InputVerifyPool::Withdraw(const std::shared_ptr<InputJob>& job)
{
    const std::scoped_lock lock{ mutex_ };
    const auto it = std::ranges::find(jobs_, job);
    if (it != jobs_.end()) {
        jobs_.erase(it);
        openJobs_.fetch_sub(1, std::memory_order_relaxed);
    }
}

size_t InputVerifyPool::QueuedJobs()
{
    const std::scoped_lock lock{ mutex_ };
    return jobs_.size();
}

bool InputVerifyPool::ReleasedCore(size_t busyAfter)
{
    if (busyAfter >= cores_ || openJobs_.load(std::memory_order_seq_cst) == 0
        || helpersOnJobs_.load(std::memory_order_seq_cst) >= HelperLimit())
        return false;
    { const std::scoped_lock lock{ mutex_ }; }  // a helper checking the budget is now waiting
    wake_.notify_one();
    return true;
}

InputVerifyPool::InputVerifyPool(size_t cores) : cores_{ cores } {}

InputVerifyPool* InputVerifyPool::Create()
{
    if (const auto hook = inputPoolCreationHook.load(std::memory_order_relaxed))
        hook();
    // Read once: a pool keeps this count, and fewer than 2 CPUs means no pool
    // for the process lifetime. Only allocation or thread-start failure retries.
    size_t cores = bsv::detail::EffectiveCpuCount();
    if (const size_t cap = inputPoolCoresCap.load(); cap != 0)
        cores = std::min(cores, cap);
    if (cores < 2) {
        tooFewCores_.store(true, std::memory_order_release);
        return nullptr;
    }
    auto pool = std::make_unique<InputVerifyPool>(cores);
    for (size_t i = 0; i + 1 < cores && StartHelper(pool.get()); ++i)
        ++pool->helpers_;
    if (pool->helpers_ == 0)
        return nullptr;
    // Detached helpers retain this address for process lifetime. No
    // throwing operation may follow their first successful start.
    auto* published = pool.release();
    created_.store(published, std::memory_order_release);
    return published;
}

bool InputVerifyPool::StartHelper(InputVerifyPool* pool)
{
#if defined(_WIN32)
    try {
        std::thread([pool] { pool->Run(); }).detach();
        return true;
    } catch (const std::system_error&) {
        return false;
    } catch (const std::bad_alloc&) {
        return false;
    }
#else
    struct Attributes {
        pthread_attr_t value{};
        const bool initialized = pthread_attr_init(&value) == 0;
        Attributes() = default;
        ~Attributes() { if (initialized) pthread_attr_destroy(&value); }
        Attributes(const Attributes&) = delete;
        Attributes& operator=(const Attributes&) = delete;
        Attributes(Attributes&&) = delete;
        Attributes& operator=(Attributes&&) = delete;
    } attributes;
    if (!attributes.initialized)
        return false;
    pthread_t thread{};
    return pthread_attr_setstacksize(&attributes.value, bsv::detail::INPUT_HELPER_STACK_BYTES) == 0
        && pthread_attr_setdetachstate(&attributes.value, PTHREAD_CREATE_DETACHED) == 0
        && pthread_create(&thread, &attributes.value, &InputVerifyPool::RunHelper, pool) == 0;
#endif
}

#if !defined(_WIN32)
void* InputVerifyPool::RunHelper(void* pool)
{
    static_cast<InputVerifyPool*>(pool)->Run();
    return nullptr;
}

#endif

bool InputVerifyPool::TryTakeHelperPlace()
{
    const size_t limit = HelperLimit();
    size_t onJobs = helpersOnJobs_.load(std::memory_order_relaxed);
    while (onJobs < limit) {
        if (helpersOnJobs_.compare_exchange_weak(onJobs, onJobs + 1, std::memory_order_relaxed))
            return true;
    }
    return false;
}

bool InputVerifyPool::TryTakeCore(size_t cores)
{
    size_t busy = busyThreads.load(std::memory_order_seq_cst);
    while (busy < cores) {
        if (busyThreads.compare_exchange_weak(busy, busy + 1, std::memory_order_seq_cst))
            return true;
    }
    return false;
}

std::shared_ptr<InputJob> InputVerifyPool::NextJob()
{
    while (!jobs_.empty() && !jobs_.front()->HasWork()) {
        jobs_.pop_front();
        openJobs_.fetch_sub(1, std::memory_order_relaxed);
    }
    std::shared_ptr<InputJob> largest;
    size_t most = 0;
    for (const auto& job : jobs_) {
        const size_t remaining = job->HelperUnclaimedInputs();
        if (remaining > most) {
            most = remaining;
            largest = job;
        }
    }
    return largest;
}

void InputVerifyPool::Run()
{
    threadIsBusy = true;  // a helper's own work never counts twice
    for (;;) {
        std::shared_ptr<InputJob> job;
        {
            std::unique_lock lock{ mutex_ };
            wake_.wait(lock, [&] {
                job = NextJob();
                while (job && TryTakeHelperPlace()) {
                    if (TryTakeCore(cores_))
                        return true;
                    if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
                        hook(bsv::detail::InputPoolEvent::CoreUnavailable);
                    // Pair the returned reservation and the capacity check
                    // with ReleasedCore's seq_cst helper-cap load. If that
                    // release saw our reserved place as full, this check
                    // sees its freed core; retry before going to sleep.
                    helpersOnJobs_.fetch_sub(1, std::memory_order_seq_cst);
                    if (inputCountersEnabled.load(std::memory_order_relaxed))
                        helperWakesWithoutCore.fetch_add(1, std::memory_order_relaxed);
                    if (busyThreads.load(std::memory_order_seq_cst) >= cores_)
                        break;
                }
                job.reset();  // don't keep a finished job alive while asleep
                if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
                    hook(bsv::detail::InputPoolEvent::Waiting);
                return false;
            });
        }
        const bool count = inputCountersEnabled.load(std::memory_order_relaxed);
        InputJob::HelperTerms terms{ .cores = cores_, .places = &helpersOnJobs_, .inputs = count ? &helperVerifiedInputs : nullptr };
        if (job->TryJoin()) {
            const size_t verified = job->Work(&terms).value_or(0);
            if (count) {
                if (verified > 0)
                    helperJobParticipations.fetch_add(1, std::memory_order_relaxed);
                jobVerifiedInputs.fetch_add(verified, std::memory_order_relaxed);
            }
            job->Leave();
        }
        job.reset();
        if (!terms.placeReturned)
            helpersOnJobs_.fetch_sub(1, std::memory_order_relaxed);
        const size_t busyAfter = busyThreads.fetch_sub(1, std::memory_order_seq_cst) - 1;
        ReleasedCore(busyAfter);
        if (const auto hook = helperReleasedCoreHook.load(std::memory_order_relaxed))
            hook();
    }
}

BusyScope::BusyScope() : counted_{ !threadIsBusy }
{
    if (counted_) {
        threadIsBusy = true;
        busyThreads.fetch_add(1, std::memory_order_seq_cst);
    }
}

BusyScope::~BusyScope()
{
    if (!counted_)
        return;
    threadIsBusy = false;
    const size_t busyAfter = busyThreads.fetch_sub(1, std::memory_order_seq_cst) - 1;
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        pool->ReleasedCore(busyAfter);
}

JobCaller::JobCaller(InputVerifyPool& pool, std::shared_ptr<InputJob> job) : pool_{ pool }, job_{ std::move(job) } {}

JobCaller::~JobCaller()
{
    if (!drained_)
        job_->CloseAndWait(false);
    pool_.Withdraw(job_);
}

void JobCaller::Finish()
{
    // Normal draining may invoke the caller's token; unwind cleanup does not.
    job_->CloseAndWait();
    drained_ = true;
}

std::atomic<InputVerifyPool*> InputVerifyPool::created_{ nullptr };
std::atomic<bool> InputVerifyPool::tooFewCores_{ false };
#endif
} // namespace bsv::detail

void bsv::SetParallelInputHelpers(int64_t helpers)
{
    bsv::detail::parallelInputHelpers.store(helpers < 0 ? -1 : helpers, std::memory_order_relaxed);
}

int64_t bsv::GetParallelInputHelpers()
{
    return bsv::detail::parallelInputHelpers.load(std::memory_order_relaxed);
}

size_t bsv::detail::SetParallelInputThresholdForTesting(size_t minInputs)
{
    return parallelInputThreshold.exchange(minInputs);
}

bsv::detail::InputPoolCreationHook bsv::detail::SetInputPoolCreationHookForTesting(InputPoolCreationHook hook)
{
    return inputPoolCreationHook.exchange(hook);
}

uint64_t bsv::detail::HelperVerifiedInputCountForTesting()
{
    return helperVerifiedInputs.load();
}

bool bsv::detail::SetInputCountersEnabledForTesting(bool enabled)
{
    return inputCountersEnabled.exchange(enabled);
}

uint64_t bsv::detail::HelperWakesWithoutCoreForTesting()
{
    return helperWakesWithoutCore.load();
}

uint64_t bsv::detail::HelperJobParticipationCountForTesting()
{
    return helperJobParticipations.load();
}

bsv::detail::InputSourceAllocationHook bsv::detail::SetInputSourceAllocationHookForTesting(InputSourceAllocationHook hook)
{
    return inputSourceAllocationHook.exchange(hook);
}

bsv::detail::ValidationStageHook bsv::detail::SetValidationStageHookForTesting(ValidationStageHook hook)
{
    return validationStageHook.exchange(hook);
}

bsv::detail::InputHook bsv::detail::SetInputHookForTesting(InputHook hook)
{
    return inputHook.exchange(hook);
}

bsv::detail::HelperReleasedCoreHook bsv::detail::SetHelperReleasedCoreHookForTesting(HelperReleasedCoreHook hook)
{
    return helperReleasedCoreHook.exchange(hook);
}

bsv::detail::InputPoolHook bsv::detail::SetInputPoolHookForTesting(InputPoolHook hook)
{
    return inputPoolHook.exchange(hook);
}

void bsv::detail::WakeInputHelpersForTesting(bool all)
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        pool->WakeHelpersForTesting(all);
#else
    (void)all;
#endif
}

uint64_t bsv::detail::JobVerifiedInputCountForTesting()
{
    return jobVerifiedInputs.load();
}

size_t bsv::detail::SetInputPoolCoresForTesting(size_t cores)
{
    return inputPoolCoresCap.exchange(cores);
}

size_t bsv::detail::BusyThreadCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    return busyThreads.load();
#else
    return 0;
#endif
}

size_t bsv::detail::QueuedJobCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        return pool->QueuedJobs();
#endif
    return 0;
}

size_t bsv::detail::InputPoolHelperCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (const InputVerifyPool* pool = InputVerifyPool::Get())
        return pool->Helpers();
#endif
    return 0;
}

void bsv::detail::AddBusyThreadsForTesting(size_t count)
{
#if BDK_PARALLEL_INPUTS
    busyThreads.fetch_add(count);
#else
    (void)count;
#endif
}

void bsv::detail::ReleaseBusyThreadsForTesting(size_t count)
{
#if BDK_PARALLEL_INPUTS
    for (size_t i = 0; i < count; ++i) {
        const size_t busyAfter = busyThreads.fetch_sub(1) - 1;
        if (InputVerifyPool* pool = InputVerifyPool::Existing())
            pool->ReleasedCore(busyAfter);
    }
#else
    (void)count;
#endif
}
