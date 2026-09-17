#include <bdkcgo/validatebatch_cgo.h>
#include <bdkcgo/src/abiguard.hpp>
#include <core/validatearg.hpp>
#include <vector>
#include <iostream>

// Internal helper to emit a diagnostic from a catch handler.
// Stream insertion and the flush in std::endl can allocate, so they can throw — and a
// catch handler that throws leaves an extern "C" function with an exception already in
// flight, which is undefined behaviour. The emission is therefore best effort: if it
// fails, the diagnostic is dropped and the caller still gets its documented result.
static void _helper_log_cgo_exception(const char* where, const char* what) noexcept
{
    try {
        std::cout << "CGO EXCEPTION : " << where << " " << what << std::endl;
    }
    catch (...) {
    }
}

ValidateBatchCGO ValidateBatch_CreateV2()
{
    try {
        return new bsv::ValidateBatch();
    }
    catch (const std::exception& e) {
        _helper_log_cgo_exception(__FILE__ " at ValidateBatch_CreateV2", e.what());
        return nullptr;
    }
    catch (...) {
        // Nothing may cross the extern "C" boundary; a non-std exception takes the
        // same channel as any other construction failure.
        _helper_log_cgo_exception(__FILE__ " at ValidateBatch_CreateV2", "unknown exception");
        return nullptr;
    }
}

void ValidateBatch_Destroy(ValidateBatchCGO cgoBatch)
{
    bsv::ValidateBatch* ptr = static_cast<bsv::ValidateBatch*>(cgoBatch);
    delete ptr;
}

TxError ValidateBatch_Add(
    ValidateBatchCGO cgoBatch,
    const char* extendedTxPtr, uint64_t extendedTxLen,
    const int32_t* hUTXOsPtr, uint64_t hUTXOsLen,
    int32_t blockHeight,
    bool consensus
)
{
    try {
        // Convert C-style parameters to C++ spans
        const std::span<const uint8_t> extendedTx = bdkcgo::byte_span(extendedTxPtr, extendedTxLen);
        const std::span<const int32_t> hUTXOs = bdkcgo::int32_span(hUTXOsPtr, hUTXOsLen);

        // Create ValidateArg and add to batch
        bsv::ValidateArg arg(extendedTx, hUTXOs, blockHeight, consensus);
        static_cast<bsv::ValidateBatch*>(cgoBatch)->add(std::move(arg));
        return bsv::TxErrorOk();
    }
    catch (const bdkcgo::AbiArgumentError& e) {
        // Nothing was appended: the caller must not carry on filling a batch whose
        // results would no longer line up with its own input list.
        return bsv::AbiErrorToTxError(e.code());
    }
    catch (const std::exception& e) {
        _helper_log_cgo_exception(__FILE__ " at ValidateBatch_Add", e.what());
        return bsv::TxErrorException();
    }
    catch (...) {
        return bsv::TxErrorException();
    }
}

void ValidateBatch_Clear(ValidateBatchCGO cgoBatch)
{
    static_cast<bsv::ValidateBatch*>(cgoBatch)->clear();
}

uint64_t ValidateBatch_Size(ValidateBatchCGO cgoBatch)
{
    return static_cast<uint64_t>(static_cast<bsv::ValidateBatch*>(cgoBatch)->size());
}

bool ValidateBatch_Empty(ValidateBatchCGO cgoBatch)
{
    return static_cast<bsv::ValidateBatch*>(cgoBatch)->empty();
}

void ValidateBatch_Reserve(ValidateBatchCGO cgoBatch, uint64_t capacity)
{
    if (capacity == 0) {
        return;
    }

    // Reserving is best effort. A capacity this platform or the container cannot hold
    // is ignored rather than clamped, and length_error / bad_alloc is swallowed here:
    // failing to honour the hint is harmless, letting an exception cross extern "C"
    // is not.
    if (capacity > static_cast<uint64_t>(std::vector<bsv::ValidateArg>().max_size())) {
        return;
    }

    try {
        static_cast<bsv::ValidateBatch*>(cgoBatch)->reserve(static_cast<size_t>(capacity));
    }
    catch (...) {
    }
}
