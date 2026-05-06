#include <bdkcgo/validatebatch_cgo.h>
#include <core/validatearg.hpp>
#include <vector>
#include <iostream>

ValidateBatchCGO ValidateBatch_Create()
{
    try {
        return new bsv::ValidateBatch();
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << " " << e.what() << std::endl;
        return nullptr;
    }
}

void ValidateBatch_Destroy(ValidateBatchCGO cgoBatch)
{
    bsv::ValidateBatch* ptr = static_cast<bsv::ValidateBatch*>(cgoBatch);
    delete ptr;
}

void ValidateBatch_Add(
    ValidateBatchCGO cgoBatch,
    const char* extendedTxPtr, int extendedTxLen,
    const int32_t* hUTXOsPtr, int hUTXOsLen,
    int32_t blockHeight,
    bool consensus
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

        // Create ValidateArg and add to batch
        bsv::ValidateArg arg(extendedTx, hUTXOs, blockHeight, consensus);
        static_cast<bsv::ValidateBatch*>(cgoBatch)->add(std::move(arg));
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << " " << e.what() << std::endl;
    }
}

void ValidateBatch_Clear(ValidateBatchCGO cgoBatch)
{
    static_cast<bsv::ValidateBatch*>(cgoBatch)->clear();
}

int ValidateBatch_Size(ValidateBatchCGO cgoBatch)
{
    return static_cast<int>(static_cast<bsv::ValidateBatch*>(cgoBatch)->size());
}

bool ValidateBatch_Empty(ValidateBatchCGO cgoBatch)
{
    return static_cast<bsv::ValidateBatch*>(cgoBatch)->empty();
}

void ValidateBatch_Reserve(ValidateBatchCGO cgoBatch, int capacity)
{
    if (capacity > 0) {
        static_cast<bsv::ValidateBatch*>(cgoBatch)->reserve(static_cast<size_t>(capacity));
    }
}
