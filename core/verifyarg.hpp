#ifndef __VERIFYARG_HPP__
#define __VERIFYARG_HPP__

#include <cstdint>
#include <span>
#include <vector>

namespace bsv
{

/**
 * VerifyArg holds all the arguments needed for a single script verification
 */
struct VerifyArg {
    std::span<const uint8_t> extendedTX;
    std::span<const int32_t> utxoHeights;
    int32_t blockHeight;
    bool consensus;
    std::span<const uint32_t> customFlags;

    VerifyArg(
        std::span<const uint8_t> extendedTX_,
        std::span<const int32_t> utxoHeights_,
        int32_t blockHeight_,
        bool consensus_,
        std::span<const uint32_t> customFlags_ = std::span<const uint32_t>()
    );
};

/**
 * VerifyBatch holds a collection of VerifyArg objects for batch processing
 *
 * This container is specifically designed for batch verification workflows where
 * operations are processed as an all-or-none unit. It provides the minimum set
 * of operations required for batch processing:
 *
 * - add(): Build up a batch of verification tasks incrementally
 * - clear(): Reset the batch for reuse or discard all tasks
 * - size()/empty(): Query batch state for validation and optimization
 * - reserve(): Pre-allocate capacity when batch size is known in advance
 * - Iteration: Process all elements sequentially during batch execution
 *
 * Deliberately excluded operations:
 * - Individual element removal: Batch processing is all-or-none; partial removal
 *   would break the batch semantics. Use clear() to reset the entire batch.
 * - Random access: Batch processing is inherently sequential. Random access would
 *   suggest individual element manipulation, which contradicts batch semantics.
 * - Element modification: Once added to the batch, elements should remain immutable
 *   until processing completes or the batch is cleared.
 *
 * This minimal interface ensures the container is used correctly for its intended
 * purpose: collecting verification tasks, processing them as a batch, and managing
 * the batch lifecycle.
 */
class VerifyBatch {
public:
    using const_iterator = std::vector<VerifyArg>::const_iterator;
    using iterator = std::vector<VerifyArg>::iterator;

    VerifyBatch() = default;

    // Add a single element to the batch
    void add(const VerifyArg& arg);
    void add(VerifyArg&& arg);

    // Clear all elements from the batch
    void clear();

    // Get the number of elements in the batch
    size_t size() const;

    // Check if the batch is empty
    bool empty() const;

    // Reserve capacity for elements
    void reserve(size_t capacity);

    // Iterator support for range-based for loops
    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

private:
    std::vector<VerifyArg> batch_;
};

} // namespace bsv

#endif /* __VERIFYARG_HPP__ */
