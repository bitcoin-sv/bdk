#pragma once

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "taskcancellation.h"

// Builds without threads (WASM without pthreads) verify inputs serially only.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define BDK_PARALLEL_INPUTS 0 // NOLINT(cppcoreguidelines-macro-usage): remove threading code in WASM.
#else
#define BDK_PARALLEL_INPUTS 1 // NOLINT(cppcoreguidelines-macro-usage): remove threading code in WASM.
#endif

// Test hooks. The hook points in namespace hooks below are empty inline
// functions, and the parallel-input threshold a constant, in every build of
// the core except bdk_core_testing: the same sources compiled with
// BDK_TESTING_HOOKS=1, linked only by the C++ tests that drive the input pool
// (core/CMakeLists.txt). Only that variant defines the hook storage, the
// counters and the *ForTesting functions (test/core/testing_hooks.cpp).
#ifndef BDK_TESTING_HOOKS
#define BDK_TESTING_HOOKS 0 // NOLINT(cppcoreguidelines-macro-usage): set to 1 only for bdk_core_testing.
#endif


namespace bsv::detail {

enum class InputEvent : uint8_t { Claimed, Verified, Failed, Polled, CallerCancelled };
using InputHook = void (*)(size_t index, InputEvent event);

// Caller-side phases before input work is offered.
enum class ValidationStage : uint8_t { Deserializing, TransactionChecks, PrecomputedData };
using ValidationStageHook = void (*)(ValidationStage stage);

// BeforeOffer runs on the caller before publishing a job. Waiting runs on a
// helper under the pool mutex, just before it waits: never block or call pool
// functions from that event. CoreUnavailable observes a reserved helper place
// after a failed core acquisition, under that mutex; only a bounded test pause
// coordinated with a release that skips the mutex is allowed there.
// JobClosed runs under the job mutex; observers must not block or call pool functions.
enum class InputPoolEvent : uint8_t { BeforeOffer, Waiting, CoreUnavailable, JobClosed };
using InputPoolHook = void (*)(InputPoolEvent event);

// Before a participant allocates its cancellation source/token; true for a
// helper, false for the caller. May throw bad_alloc to test fallback.
using InputSourceAllocationHook = void (*)(bool helper);

// Called by a helper after it releases its core and before it checks the
// queue again, outside the pool mutex.
using HelperReleasedCoreHook = void (*)();

// Before allocating a new pool; may throw bad_alloc to exercise serial fallback.
using InputPoolCreationHook = void (*)();

// Input count from which implVerifyScript verifies a transaction's inputs on
// several threads, unless a test changes it.
inline constexpr size_t DEFAULT_PARALLEL_INPUT_THRESHOLD = 16;

// Helper threads of the input pool are created with this stack size where
// the platform lets BDK choose it (POSIX), rather than the platform default.
inline constexpr size_t INPUT_HELPER_STACK_BYTES = size_t{ 8 } << 20U;

// The points where validation code reports to, or takes a setting from, a test.
namespace hooks {
#if BDK_TESTING_HOOKS
size_t ParallelInputThreshold();
InputHook Input();  // the input hook a participant captures for its polls
void OnInput(size_t index, InputEvent event);
void OnStage(ValidationStage stage);
void OnPool(InputPoolEvent event);
void OnSourceAllocation(bool helper);
void OnHelperReleasedCore();
void OnPoolCreation();
size_t PoolCoresCap();  // 0: no cap
bool HelperStartsFail();
void CountHelperInput();
void CountJobInputs(size_t inputs, bool helper);
void CountHelperWakeWithoutCore();
#else
constexpr size_t ParallelInputThreshold() { return DEFAULT_PARALLEL_INPUT_THRESHOLD; }
inline void OnInput(size_t, InputEvent) {}
inline void OnStage(ValidationStage) {}
inline void OnPool(InputPoolEvent) {}
inline void OnSourceAllocation(bool) {}
inline void OnHelperReleasedCore() {}
inline void OnPoolCreation() {}
constexpr size_t PoolCoresCap() { return 0; }
constexpr bool HelperStartsFail() { return false; }
inline void CountHelperInput() {}
inline void CountJobInputs(size_t, bool) {}
inline void CountHelperWakeWithoutCore() {}
#endif
} // namespace hooks

// One source per job participant, reused for its successive inputs. The index
// is private to that participant; the job owns lowestFailure and waits for all
// participants before the job can go away. Only the caller participant receives
// the caller token: helpers read the cancellation flag it publishes. Internal only.
class InputCancellationSource final : public task::CCancellationSource {
public:
    InputCancellationSource(const std::atomic<size_t>& lowestFailure,
                            std::atomic<bool>& callerCancelled,
                            const task::CCancellationToken* callerToken = nullptr)
        : lowestFailure_{ lowestFailure }, callerCancelled_{ callerCancelled },
          callerToken_{ callerToken } {}

    void SetIndex(size_t index) { index_ = index; }
    bool IsCanceled() override
    {
        Notify(index_, InputEvent::Polled);
        if (callerToken_ && callerToken_->IsCanceled()) {
            callerCancelled_.store(true, std::memory_order_relaxed);
            Notify(index_, InputEvent::CallerCancelled);
        }
        return callerCancelled_.load(std::memory_order_relaxed)
            || lowestFailure_.load(std::memory_order_relaxed) < index_;
    }
    // Reports to the input hook captured when this participant started, so
    // a test's hook sees its polls until that work drains.
    void Notify([[maybe_unused]] size_t index, [[maybe_unused]] InputEvent event) const
    {
#if BDK_TESTING_HOOKS
        if (hook_)
            hook_(index, event);
#endif
    }

private:
    const std::atomic<size_t>& lowestFailure_;
    std::atomic<bool>& callerCancelled_;
    const task::CCancellationToken* const callerToken_;
#if BDK_TESTING_HOOKS
    const InputHook hook_{ hooks::Input() };
#endif
    size_t index_{ 0 };
};

#if BDK_TESTING_HOOKS
// Sets the input count from which a transaction's inputs are verified on
// several threads, process-wide, and returns the previous value.
// SIZE_MAX forces the serial path; 2 forces the parallel path for every
// multi-input transaction.
size_t SetParallelInputThresholdForTesting(size_t minInputs);

// Number of inputs verified on pool helper threads since process start,
// counted as each one finishes.
uint64_t HelperVerifiedInputCountForTesting();

// Times a woken helper found a job with inputs left but no free core, since
// process start.
uint64_t HelperWakesWithoutCoreForTesting();

// Number of inputs verified inside parallel jobs, by callers and helpers,
// since process start.
uint64_t JobVerifiedInputCountForTesting();

// Times a helper's Work() call on a job verified at least one input, since
// process start. A job with every helper on it adds one per helper.
uint64_t HelperJobParticipationCountForTesting();

// Called on the verifying thread with an input's index when a participant of
// a parallel job claims it (Claimed), completes a check (Verified), fails
// (Failed), polls its input token (Polled), or publishes caller cancellation
// (CallerCancelled); nullptr for none. The poll hook is captured per
// participant and lives until that work drains. Lets a test fix the order in
// which failures reach a job, or make inputs take longer without using CPU.
// Returns the previous hook.
InputHook SetInputHookForTesting(InputHook hook);

InputSourceAllocationHook SetInputSourceAllocationHookForTesting(InputSourceAllocationHook hook);
ValidationStageHook SetValidationStageHookForTesting(ValidationStageHook hook);
HelperReleasedCoreHook SetHelperReleasedCoreHookForTesting(HelperReleasedCoreHook hook);
InputPoolHook SetInputPoolHookForTesting(InputPoolHook hook);
// Wake existing helpers so a test can observe each one's Waiting event.
void WakeInputHelpersForTesting(bool all = true);

// Caps the cores the input pool sizes itself to (0: no cap) and returns the
// previous cap. Takes effect only when set before the pool reads the CPU
// count, so a test binary sets it at start-up to reproduce a CI-sized pool on
// a bigger machine.
size_t SetInputPoolCoresForTesting(size_t cores);

// Helper threads in the input pool, creating the pool if it does not exist
// yet; 0 without a pool.
size_t InputPoolHelperCountForTesting();

InputPoolCreationHook SetInputPoolCreationHookForTesting(InputPoolCreationHook hook);

// While set, every helper thread start fails, as at a thread or process
// limit. Returns the previous setting.
bool SetHelperStartsFailForTesting(bool fail);

// Threads currently counted against the core budget: callers inside
// VerifyScript, ValidateTransaction or VerifySpend plus helpers on a job.
size_t BusyThreadCountForTesting();

// Jobs queued in the input pool (0 without a pool).
size_t QueuedJobCountForTesting();

// Count `count` extra threads against the core budget, then release them the
// way a caller leaving does (waking a helper per freed core).
// ScopedBusyThreadsForTesting pairs them.
void AddBusyThreadsForTesting(size_t count);
void ReleaseBusyThreadsForTesting(size_t count);

class ScopedBusyThreadsForTesting {
public:
    explicit ScopedBusyThreadsForTesting(size_t count) : count_{ count } { AddBusyThreadsForTesting(count_); }
    ~ScopedBusyThreadsForTesting() { Release(); }
    void Release()
    {
        ReleaseBusyThreadsForTesting(count_);
        count_ = 0;
    }
    ScopedBusyThreadsForTesting(const ScopedBusyThreadsForTesting&) = delete;
    ScopedBusyThreadsForTesting& operator=(const ScopedBusyThreadsForTesting&) = delete;
    ScopedBusyThreadsForTesting(ScopedBusyThreadsForTesting&&) = delete;
    ScopedBusyThreadsForTesting& operator=(ScopedBusyThreadsForTesting&&) = delete;

private:
    size_t count_;
};
#endif

} // namespace bsv::detail
