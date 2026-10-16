#ifndef __TXVALIDATOR_INTERNAL_HPP__
#define __TXVALIDATOR_INTERNAL_HPP__

// Internal to bdk_core and its tests. Not installed with the core headers
// (core/bdk-core-recipe.cmake leaves core/internal/ out of the install list)
// and not part of any language binding.

#include <cstddef>
#include <cstdint>

namespace bsv::detail {

// Sets the input count from which a transaction's inputs are verified on
// several threads, process-wide, and returns the previous value.
// SIZE_MAX forces the serial path; 2 forces the parallel path for every
// multi-input transaction. Tests only.
size_t SetParallelInputThresholdForTesting(size_t minInputs);

// Number of inputs verified on pool helper threads since process start. Tests only.
uint64_t HelperVerifiedInputCountForTesting();

} // namespace bsv::detail

#endif /* __TXVALIDATOR_INTERNAL_HPP__ */
