#include <bdkcgo/verifybatch_cgo.h>
#include <core/verifyarg.hpp>
#include <vector>
#include <iostream>

VerifyBatchCGO VerifyBatch_Create()
{
    try {
        return new bsv::VerifyBatch();
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << " " << e.what() << std::endl;
        return nullptr;
    }
}

void VerifyBatch_Destroy(VerifyBatchCGO cgoBatch)
{
    bsv::VerifyBatch* ptr = static_cast<bsv::VerifyBatch*>(cgoBatch);
    delete ptr;
}

void VerifyBatch_Add(
    VerifyBatchCGO cgoBatch,
    const char* extendedTxPtr, int extendedTxLen,
    const int32_t* hUTXOsPtr, int hUTXOsLen,
    int32_t blockHeight,
    bool consensus,
    const uint32_t* cFlagsPtr, int cFlagsLen
)
{
    try {
        // Convert C-style parameters to C++ spans
        std::span<const uint8_t> extendedTx;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs;
        if (hUTXOsPtr != nullptr && hUTXOsLen > 0) {
            hUTXOs = std::span<const int32_t>(hUTXOsPtr, static_cast<size_t>(hUTXOsLen));
        }

        std::span<const uint32_t> cFlags;
        if (cFlagsPtr != nullptr && cFlagsLen > 0) {
            cFlags = std::span<const uint32_t>(cFlagsPtr, static_cast<size_t>(cFlagsLen));
        }

        // Create VerifyArg and add to batch
        bsv::VerifyArg arg(extendedTx, hUTXOs, blockHeight, consensus, cFlags);
        static_cast<bsv::VerifyBatch*>(cgoBatch)->add(std::move(arg));
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << " " << e.what() << std::endl;
    }
}

void VerifyBatch_Clear(VerifyBatchCGO cgoBatch)
{
    static_cast<bsv::VerifyBatch*>(cgoBatch)->clear();
}

int VerifyBatch_Size(VerifyBatchCGO cgoBatch)
{
    return static_cast<int>(static_cast<bsv::VerifyBatch*>(cgoBatch)->size());
}

bool VerifyBatch_Empty(VerifyBatchCGO cgoBatch)
{
    return static_cast<bsv::VerifyBatch*>(cgoBatch)->empty();
}

void VerifyBatch_Reserve(VerifyBatchCGO cgoBatch, int capacity)
{
    if (capacity > 0) {
        static_cast<bsv::VerifyBatch*>(cgoBatch)->reserve(static_cast<size_t>(capacity));
    }
}
