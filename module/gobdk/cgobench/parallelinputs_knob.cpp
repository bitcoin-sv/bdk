//go:build bdk_internal_hooks

#include <internal/txvalidator_internal.hpp>

extern "C" size_t cgobench_set_parallel_input_threshold(size_t minInputs)
{
    return bsv::detail::SetParallelInputThresholdForTesting(minInputs);
}
