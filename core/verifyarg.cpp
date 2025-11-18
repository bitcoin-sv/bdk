#include <verifyarg.hpp>

bsv::VerifyArg::VerifyArg(
    std::span<const uint8_t> extendedTX_,
    std::span<const int32_t> utxoHeights_,
    int32_t blockHeight_,
    bool consensus_,
    std::span<const uint32_t> customFlags_
) : extendedTX(extendedTX_),
    utxoHeights(utxoHeights_),
    blockHeight(blockHeight_),
    consensus(consensus_),
    customFlags(customFlags_)
{
}

// VerifyBatch implementation

void bsv::VerifyBatch::add(const VerifyArg& arg)
{
    batch_.push_back(arg);
}

void bsv::VerifyBatch::add(VerifyArg&& arg)
{
    batch_.push_back(std::move(arg));
}

void bsv::VerifyBatch::clear()
{
    batch_.clear();
}

size_t bsv::VerifyBatch::size() const
{
    return batch_.size();
}

bool bsv::VerifyBatch::empty() const
{
    return batch_.empty();
}

void bsv::VerifyBatch::reserve(size_t capacity)
{
    batch_.reserve(capacity);
}

bsv::VerifyBatch::const_iterator bsv::VerifyBatch::begin() const
{
    return batch_.begin();
}

bsv::VerifyBatch::const_iterator bsv::VerifyBatch::end() const
{
    return batch_.end();
}

bsv::VerifyBatch::iterator bsv::VerifyBatch::begin()
{
    return batch_.begin();
}

bsv::VerifyBatch::iterator bsv::VerifyBatch::end()
{
    return batch_.end();
}
