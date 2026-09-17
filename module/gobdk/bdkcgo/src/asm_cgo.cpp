#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include <script.h>
#include <core/assembler.h>
#include <bdkcgo/asm_cgo.h>
#include <bdkcgo/src/abiguard.hpp>

namespace {

// Both entry points hand the caller a malloc'd blob that the Go side releases with
// C.free, so every allocation on every path — including the diagnostic message of the
// error path — has to come from malloc.
//
// The message is a compile-time literal rather than something formatted at failure
// time: this runs from a catch handler, where the exception being handled may well be
// std::bad_alloc, and a handler that throws would leave an extern "C" function while
// an exception is already in flight. Nothing here can throw; a failed malloc returns
// null, which is an outcome these paths already define (the Go side reads it as an
// empty result).
char* _helper_malloc_literal(const char* msg) noexcept
{
    const size_t nbBytes = std::strlen(msg) + 1;
    char* out = static_cast<char*>(malloc(nbBytes));
    if (out == nullptr) {
        return nullptr;
    }
    std::memcpy(out, msg, nbBytes);
    return out;
}

} // namespace

const char* cgo_from_asm_v2(const char* asmPtr, uint64_t asmLen, uint64_t* scriptLen)
{
    // Check the out-parameter before writing through it, then make it defined on
    // every exit: a caller must never have to guess whether the length was written.
    if (scriptLen == nullptr) {
        return nullptr;
    }
    *scriptLen = 0;

    try {
        const std::span<const uint8_t> asmSpan = bdkcgo::byte_span(asmPtr, asmLen);
        std::string asmStr;
        if (!asmSpan.empty()) {
            asmStr.assign(reinterpret_cast<const char*>(asmSpan.data()), asmSpan.size());
        }

        auto script = bsv::from_asm(asmStr);
        if (script.size() == 0) {
            return nullptr;
        }

        char* scriptPtr = static_cast<char*>(malloc(script.size()));
        if (scriptPtr == nullptr) {
            return nullptr;
        }
        std::memcpy(scriptPtr, script.data(), script.size());
        *scriptLen = static_cast<uint64_t>(script.size());
        return scriptPtr;
    } catch (...) {
        // Catch-all: nothing may cross the extern "C" boundary. The diagnostic blob is
        // this function's only failure channel, and its content does not depend on the
        // exception, so every category takes the same path.
        return _helper_malloc_literal("CGO EXCEPTION : " __FILE__ "    at cgo_from_asm_v2\n");
    }
}

const char* cgo_to_asm_v2(const char* scriptPtr, uint64_t scriptLen)
{
    try {
        const std::span<const uint8_t> script = bdkcgo::byte_span(scriptPtr, scriptLen);
        auto asmStr = bsv::to_asm(script);
        auto asmLen = asmStr.size()+1; // extra null terminator in C-String

        char* asmPtr = static_cast<char*>(malloc(asmLen));
        if (asmPtr == nullptr) {
            return nullptr;
        }
        std::memcpy(asmPtr, asmStr.c_str(), asmLen);
        return asmPtr;
    } catch (...) {
        // Catch-all: nothing may cross the extern "C" boundary. The diagnostic blob is
        // this function's only failure channel, and its content does not depend on the
        // exception, so every category takes the same path.
        return _helper_malloc_literal("CGO EXCEPTION : " __FILE__ "    at cgo_to_asm_v2\n");
    }
}
