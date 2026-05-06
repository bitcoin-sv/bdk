#include <validatearg.hpp>

bsv::ValidateArg::ValidateArg(
    std::span<const uint8_t> extendedTX_,
    std::span<const int32_t> utxoHeights_,
    int32_t blockHeight_,
    bool consensus_
) : extendedTX(extendedTX_),
    utxoHeights(utxoHeights_),
    blockHeight(blockHeight_),
    consensus(consensus_)
{
}

// ValidateBatch implementation

void bsv::ValidateBatch::add(const ValidateArg& arg)
{
    batch_.push_back(arg);
}

void bsv::ValidateBatch::add(ValidateArg&& arg)
{
    batch_.push_back(std::move(arg));
}

void bsv::ValidateBatch::clear()
{
    batch_.clear();
}

size_t bsv::ValidateBatch::size() const
{
    return batch_.size();
}

bool bsv::ValidateBatch::empty() const
{
    return batch_.empty();
}

void bsv::ValidateBatch::reserve(size_t capacity)
{
    batch_.reserve(capacity);
}

bsv::ValidateBatch::const_iterator bsv::ValidateBatch::begin() const
{
    return batch_.begin();
}

bsv::ValidateBatch::const_iterator bsv::ValidateBatch::end() const
{
    return batch_.end();
}

bsv::ValidateBatch::iterator bsv::ValidateBatch::begin()
{
    return batch_.begin();
}

bsv::ValidateBatch::iterator bsv::ValidateBatch::end()
{
    return batch_.end();
}
