#include <bdkffi/validatebatch.h>

#include <core/validatearg.hpp>

#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>

namespace {

void log_exception(const char* function_name, const char* message)
{
    try {
        std::cerr << "bdkffi exception in " << function_name << ": " << message << std::endl;
    } catch (...) {
    }
}

bsv::ValidateBatch* checked_batch(bdkffi_validatebatch_t batch)
{
    if (batch == nullptr) {
        throw std::invalid_argument("null validate batch");
    }
    return static_cast<bsv::ValidateBatch*>(batch);
}

std::span<const uint8_t> byte_span(const char* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative transaction length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null transaction pointer with positive length");
    }
    return {reinterpret_cast<const uint8_t*>(ptr), static_cast<size_t>(len)};
}

std::span<const int32_t> int32_span(const int32_t* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative utxo height length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null utxo height pointer with positive length");
    }
    return {ptr, static_cast<size_t>(len)};
}

} // namespace

extern "C" bdkffi_validatebatch_t bdkffi_validatebatch_create(void)
{
    try {
        return new bsv::ValidateBatch();
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        return nullptr;
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        return nullptr;
    }
}

extern "C" void bdkffi_validatebatch_destroy(bdkffi_validatebatch_t batch)
{
    try {
        delete static_cast<bsv::ValidateBatch*>(batch);
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
    }
}

extern "C" void bdkffi_validatebatch_add(
    bdkffi_validatebatch_t batch,
    const char* extended_tx,
    int extended_tx_len,
    const int32_t* utxo_heights,
    int utxo_heights_len,
    int32_t block_height,
    bool consensus)
{
    try {
        bsv::ValidateArg arg(
            byte_span(extended_tx, extended_tx_len),
            int32_span(utxo_heights, utxo_heights_len),
            block_height,
            consensus);
        checked_batch(batch)->add(std::move(arg));
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
    }
}

extern "C" void bdkffi_validatebatch_clear(bdkffi_validatebatch_t batch)
{
    try {
        checked_batch(batch)->clear();
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
    }
}

extern "C" int bdkffi_validatebatch_size(bdkffi_validatebatch_t batch)
{
    try {
        return static_cast<int>(checked_batch(batch)->size());
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        return 0;
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        return 0;
    }
}

extern "C" bool bdkffi_validatebatch_empty(bdkffi_validatebatch_t batch)
{
    try {
        return checked_batch(batch)->empty();
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        return true;
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        return true;
    }
}

extern "C" void bdkffi_validatebatch_reserve(bdkffi_validatebatch_t batch, int capacity)
{
    try {
        if (capacity > 0) {
            checked_batch(batch)->reserve(static_cast<size_t>(capacity));
        }
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
    }
}
