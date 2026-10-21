#pragma once

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "taskcancellation.h"

namespace bsv::detail {

enum class InputEvent { Claimed, Verified, Failed, Polled, CallerCancelled };
using InputHook = void (*)(size_t index, InputEvent event);

// One source per job participant, reused for its successive inputs. The index
// is private to that participant; the job owns lowestFailure and waits for all
// participants before the job can go away. Only the caller participant receives
// the caller token: helpers read the cancellation flag it publishes. Internal only.
class InputCancellationSource final : public task::CCancellationSource {
public:
    InputCancellationSource(const std::atomic<size_t>& lowestFailure,
                            std::atomic<bool>& callerCancelled,
                            const task::CCancellationToken* callerToken = nullptr,
                            InputHook hook = nullptr)
        : lowestFailure_{ lowestFailure }, callerCancelled_{ callerCancelled },
          callerToken_{ callerToken }, hook_{ hook } {}

    void SetIndex(size_t index) { index_ = index; }
    bool IsCanceled() override
    {
        if (hook_)
            hook_(index_, InputEvent::Polled);
        if (callerToken_ && callerToken_->IsCanceled()) {
            callerCancelled_.store(true, std::memory_order_relaxed);
            if (hook_)
                hook_(index_, InputEvent::CallerCancelled);
        }
        return callerCancelled_.load(std::memory_order_relaxed)
            || lowestFailure_.load(std::memory_order_relaxed) < index_;
    }

private:
    const std::atomic<size_t>& lowestFailure_;
    std::atomic<bool>& callerCancelled_;
    const task::CCancellationToken* const callerToken_;
    const InputHook hook_;
    size_t index_{ 0 };
};

// Input count from which implVerifyScript verifies a transaction's inputs on
// several threads, unless a test changes it.
inline constexpr size_t DEFAULT_PARALLEL_INPUT_THRESHOLD = 16;

// Sets the input count from which a transaction's inputs are verified on
// several threads, process-wide, and returns the previous value.
// SIZE_MAX forces the serial path; 2 forces the parallel path for every
// multi-input transaction. Tests only.
size_t SetParallelInputThresholdForTesting(size_t minInputs);

// Helper threads of the input pool are created with this stack size where
// the platform lets BDK choose it (POSIX), rather than the platform default.
inline constexpr size_t INPUT_HELPER_STACK_BYTES = size_t{ 8 } << 20;

// The counters below (helper inputs, job inputs, helper participations) count
// only while enabled; off by default, so production does no per-input write
// to them. Returns the previous setting. Tests only.
bool SetInputCountersEnabledForTesting(bool enabled);

class ScopedInputCountersForTesting {
public:
    explicit ScopedInputCountersForTesting(bool enabled) : previous_{ SetInputCountersEnabledForTesting(enabled) } {}
    ~ScopedInputCountersForTesting() { SetInputCountersEnabledForTesting(previous_); }
    ScopedInputCountersForTesting(const ScopedInputCountersForTesting&) = delete;
    ScopedInputCountersForTesting& operator=(const ScopedInputCountersForTesting&) = delete;

private:
    const bool previous_;
};

// Number of inputs verified on pool helper threads since process start,
// counted as each one finishes, while counting is enabled. Tests only.
uint64_t HelperVerifiedInputCountForTesting();

// Times a woken helper found a job with inputs left but no free core, since
// process start, while counting is enabled. Tests only.
uint64_t HelperWakesWithoutCoreForTesting();

// ReleasedCore notifications while counting is enabled. Tests only.
uint64_t HelperReleaseNotificationsForTesting();

// Number of inputs verified inside parallel jobs, by callers and helpers,
// since process start. Tests only.
uint64_t JobVerifiedInputCountForTesting();

// Times a helper's Work() call on a job verified at least one input, since
// process start. A job with every helper on it adds one per helper. Tests only.
uint64_t HelperJobParticipationCountForTesting();

// Input cancellation sources constructed by Work(), since process start,
// while input counting is enabled. Tests only.
uint64_t InputCancellationSourceCountForTesting();

// Called on the verifying thread with an input's index when a participant of
// a parallel job claims it (Claimed), completes a check (Verified), fails
// (Failed), polls its input token
// (Polled), or publishes caller cancellation (CallerCancelled); nullptr for none.
// The poll hook is captured per participant and lives until that work drains.
// Lets a test fix
// the order in which failures reach a job, or make inputs take longer
// without using CPU. Returns the previous hook. Tests only.
InputHook SetInputHookForTesting(InputHook hook);

// Before a participant allocates its cancellation source/token; true for a
// helper, false for the caller. May throw bad_alloc to test fallback. Tests only.
using InputSourceAllocationHook = void (*)(bool helper);
InputSourceAllocationHook SetInputSourceAllocationHookForTesting(InputSourceAllocationHook hook);

// Caller-side phases before input work is offered. Internal observer for tests.
enum class ValidationStage { Deserializing, TransactionChecks, PrecomputedData };
using ValidationStageHook = void (*)(ValidationStage stage);
ValidationStageHook SetValidationStageHookForTesting(ValidationStageHook hook);

// Called by a helper after it releases its core and before it checks the
// queue again, outside the pool mutex. Returns the previous hook. Tests only.
using HelperReleasedCoreHook = void (*)();
HelperReleasedCoreHook SetHelperReleasedCoreHookForTesting(HelperReleasedCoreHook hook);

// BeforeOffer runs on the caller before publishing a job. Waiting runs on a
// helper under the pool mutex, just before it waits: never block or call pool
// functions from that event. CoreUnavailable observes a reserved helper place
// after a failed core acquisition, under that mutex; only a bounded test pause
// coordinated with a release that skips the mutex is allowed there.
// Returns the previous hook. Tests only.
enum class InputPoolEvent { BeforeOffer, Waiting, CoreUnavailable };
using InputPoolHook = void (*)(InputPoolEvent event);
InputPoolHook SetInputPoolHookForTesting(InputPoolHook hook);
// Wake existing helpers so a test can observe each one's Waiting event.
void WakeInputHelpersForTesting(bool all = true);

// Caps the cores the input pool sizes itself to (0: no cap) and returns the
// previous cap. Takes effect only when set before the pool is created, so a
// test binary sets it at start-up to reproduce a CI-sized pool on a bigger
// machine. Tests only.
size_t SetInputPoolCoresForTesting(size_t cores);

// Helper threads in the input pool, creating the pool if it does not exist
// yet; 0 without a pool. Tests only.
size_t InputPoolHelperCountForTesting();

// Threads currently counted against the core budget: callers inside
// VerifyScript, ValidateTransaction or VerifySpend plus helpers on a job. Tests only.
size_t BusyThreadCountForTesting();

// Jobs queued in the input pool (0 without a pool). Tests only.
size_t QueuedJobCountForTesting();

// Count `count` extra threads against the core budget, then release them the
// way a caller leaving does (waking a helper per freed core). Tests only;
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

private:
    size_t count_;
};

// The CPU bandwidth limit (quota / period, in CPUs) of the cgroup this process
// is in, read from the files under `root` ("/" on a live system): cgroup v2
// cpu.max, or cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us of the hierarchy
// with the cpu controller, located through proc/self/cgroup and
// proc/self/mountinfo. nullopt when there is no limit, or when any file is
// missing or does not parse. The lookup follows Go 1.26's
// internal/runtime/cgroup (FindCPUCgroup, FindCPUMountPoint).
std::optional<double> CgroupCpuLimit(const std::string& root);

// Lowers a CPU count to a cgroup limit the way Go sets its default GOMAXPROCS
// (runtime/cgroup_linux.go, adjustCgroupGOMAXPROCS, the same from go1.25.0 on):
// the limit is rounded up to a whole CPU and to at least 2, and only ever
// lowers `cpus`.
size_t LimitCpuCount(size_t cpus, std::optional<double> cgroupLimit);

// CPUs this process may use: hardware_concurrency(), lowered on Linux to the
// calling thread's sched_getaffinity mask (so the input pool takes the mask
// of the thread that creates it, the first caller with a large tx) and, by
// LimitCpuCount, to the cgroup limit read under `cgroupRoot` ("/" on a live
// system; tests pass a fixture root). Reads the system on every call; the
// input pool calls it once.
size_t EffectiveCpuCount(const std::string& cgroupRoot = "/");

} // namespace bsv::detail
