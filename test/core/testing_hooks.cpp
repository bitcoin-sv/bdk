// Hook storage, counters and *ForTesting functions for the input pool tests.
// Compiled only into bdk_core_testing (core/CMakeLists.txt), the core variant
// built from the same sources with BDK_TESTING_HOOKS=1; bdk_core and every
// library made from it have none of these symbols.

#include <internal/input_verification.hpp>
#include <internal/txvalidator_internal.hpp>

#include <atomic>
#include <cstdint>

#if !BDK_TESTING_HOOKS
#error "testing_hooks.cpp belongs to bdk_core_testing only"
#endif

namespace bsv::detail {
namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): process-wide test hooks, atomic by design
std::atomic<size_t> parallelInputThreshold{ DEFAULT_PARALLEL_INPUT_THRESHOLD };
std::atomic<uint64_t> helperVerifiedInputs{ 0 };
std::atomic<uint64_t> jobVerifiedInputs{ 0 };  // caller and helpers, inside jobs
std::atomic<uint64_t> helperJobParticipations{ 0 };  // helper Work() calls that verified an input
std::atomic<uint64_t> helperWakesWithoutCore{ 0 };
std::atomic<InputHook> inputHook{ nullptr };
std::atomic<InputSourceAllocationHook> inputSourceAllocationHook{ nullptr };
std::atomic<ValidationStageHook> validationStageHook{ nullptr };
std::atomic<HelperReleasedCoreHook> helperReleasedCoreHook{ nullptr };
std::atomic<InputPoolHook> inputPoolHook{ nullptr };
std::atomic<size_t> inputPoolCoresCap{ 0 };  // 0: no cap
std::atomic<InputPoolCreationHook> inputPoolCreationHook{ nullptr };
std::atomic<bool> helperStartsFail{ false };
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

size_t hooks::ParallelInputThreshold() { return parallelInputThreshold.load(std::memory_order_relaxed); }

InputHook hooks::Input() { return inputHook.load(std::memory_order_relaxed); }

void hooks::OnInput(size_t index, InputEvent event)
{
    if (const auto hook = inputHook.load(std::memory_order_relaxed))
        hook(index, event);
}

void hooks::OnStage(ValidationStage stage)
{
    if (const auto hook = validationStageHook.load(std::memory_order_relaxed))
        hook(stage);
}

void hooks::OnPool(InputPoolEvent event)
{
    if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
        hook(event);
}

void hooks::OnSourceAllocation(bool helper)
{
    if (const auto hook = inputSourceAllocationHook.load(std::memory_order_relaxed))
        hook(helper);
}

void hooks::OnHelperReleasedCore()
{
    if (const auto hook = helperReleasedCoreHook.load(std::memory_order_relaxed))
        hook();
}

void hooks::OnPoolCreation()
{
    if (const auto hook = inputPoolCreationHook.load(std::memory_order_relaxed))
        hook();
}

size_t hooks::PoolCoresCap() { return inputPoolCoresCap.load(); }

bool hooks::HelperStartsFail() { return helperStartsFail.load(std::memory_order_relaxed); }

void hooks::CountHelperInput() { helperVerifiedInputs.fetch_add(1, std::memory_order_relaxed); }

void hooks::CountJobInputs(size_t inputs, bool helper)
{
    if (helper && inputs > 0)
        helperJobParticipations.fetch_add(1, std::memory_order_relaxed);
    jobVerifiedInputs.fetch_add(inputs, std::memory_order_relaxed);
}

void hooks::CountHelperWakeWithoutCore() { helperWakesWithoutCore.fetch_add(1, std::memory_order_relaxed); }

size_t SetParallelInputThresholdForTesting(size_t minInputs) { return parallelInputThreshold.exchange(minInputs); }

InputPoolCreationHook SetInputPoolCreationHookForTesting(InputPoolCreationHook hook)
{
    return inputPoolCreationHook.exchange(hook);
}

bool SetHelperStartsFailForTesting(bool fail) { return helperStartsFail.exchange(fail); }

uint64_t HelperVerifiedInputCountForTesting() { return helperVerifiedInputs.load(); }

uint64_t HelperWakesWithoutCoreForTesting() { return helperWakesWithoutCore.load(); }

uint64_t HelperJobParticipationCountForTesting() { return helperJobParticipations.load(); }

uint64_t JobVerifiedInputCountForTesting() { return jobVerifiedInputs.load(); }

InputSourceAllocationHook SetInputSourceAllocationHookForTesting(InputSourceAllocationHook hook)
{
    return inputSourceAllocationHook.exchange(hook);
}

ValidationStageHook SetValidationStageHookForTesting(ValidationStageHook hook)
{
    return validationStageHook.exchange(hook);
}

InputHook SetInputHookForTesting(InputHook hook) { return inputHook.exchange(hook); }

HelperReleasedCoreHook SetHelperReleasedCoreHookForTesting(HelperReleasedCoreHook hook)
{
    return helperReleasedCoreHook.exchange(hook);
}

InputPoolHook SetInputPoolHookForTesting(InputPoolHook hook) { return inputPoolHook.exchange(hook); }

size_t SetInputPoolCoresForTesting(size_t cores) { return inputPoolCoresCap.exchange(cores); }

void WakeInputHelpersForTesting(bool all)
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        pool->WakeHelpersForTesting(all);
#else
    (void)all;
#endif
}

size_t BusyThreadCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    return busyThreads.load();
#else
    return 0;
#endif
}

size_t QueuedJobCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        return pool->QueuedJobs();
#endif
    return 0;
}

size_t InputPoolHelperCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (const InputVerifyPool* pool = InputVerifyPool::Get())
        return pool->Helpers();
#endif
    return 0;
}

void AddBusyThreadsForTesting(size_t count)
{
#if BDK_PARALLEL_INPUTS
    busyThreads.fetch_add(count);
#else
    (void)count;
#endif
}

void ReleaseBusyThreadsForTesting(size_t count)
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
} // namespace bsv::detail
