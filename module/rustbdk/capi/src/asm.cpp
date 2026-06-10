#include <bdkffi/asm.h>

#include <script/script.h>
#include <core/assembler.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>

namespace {

void log_exception(const char* function_name, const char* message)
{
    try {
        std::cerr << "bdkffi exception in " << function_name << ": " << message << std::endl;
    } catch (...) {
    }
}

std::span<const uint8_t> byte_span(const char* ptr, int len)
{
    if (len < 0) {
        throw std::invalid_argument("negative buffer length");
    }
    if (len == 0) {
        return {};
    }
    if (ptr == nullptr) {
        throw std::invalid_argument("null buffer with positive length");
    }
    return {reinterpret_cast<const uint8_t*>(ptr), static_cast<size_t>(len)};
}

char* malloc_bytes(const uint8_t* data, size_t len)
{
    if (len == 0) {
        return nullptr;
    }
    char* out = static_cast<char*>(std::malloc(len));
    if (out == nullptr) {
        return nullptr;
    }
    std::memcpy(out, data, len);
    return out;
}

char* malloc_c_string(const std::string& value)
{
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (out == nullptr) {
        return nullptr;
    }
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

} // namespace

extern "C" char* bdkffi_from_asm(const char* asm_ptr, int asm_len, int* script_len)
{
    try {
        if (script_len != nullptr) {
            *script_len = 0;
        }
        if (asm_len < 0) {
            throw std::invalid_argument("negative asm length");
        }
        if (asm_len > 0 && asm_ptr == nullptr) {
            throw std::invalid_argument("null asm pointer with positive length");
        }

        const std::string asm_string(asm_ptr == nullptr ? "" : asm_ptr, static_cast<size_t>(asm_len));
        const auto script = bsv::from_asm(asm_string);
        if (script_len != nullptr) {
            *script_len = static_cast<int>(script.size());
        }
        return malloc_bytes(script.data(), script.size());
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        if (script_len != nullptr) {
            *script_len = 0;
        }
        return nullptr;
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        if (script_len != nullptr) {
            *script_len = 0;
        }
        return nullptr;
    }
}

extern "C" char* bdkffi_to_asm(const char* script_ptr, int script_len)
{
    try {
        const auto script = byte_span(script_ptr, script_len);
        return malloc_c_string(bsv::to_asm(script));
    } catch (const std::exception& e) {
        log_exception(__func__, e.what());
        return nullptr;
    } catch (...) {
        log_exception(__func__, "unknown C++ exception");
        return nullptr;
    }
}
