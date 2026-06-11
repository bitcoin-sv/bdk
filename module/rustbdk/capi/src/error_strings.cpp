#include <bdkffi/error_strings.h>

#include <core/doserror.hpp>
#include <core/txvalidator.hpp>
#include <script/script_error.h>

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

char* copy_c_string(const char* s)
{
    if (s == nullptr) {
        s = "";
    }

    const auto len = std::strlen(s);
    auto* out = static_cast<char*>(std::malloc(len + 1));
    if (out == nullptr) {
        return nullptr;
    }

    std::memcpy(out, s, len + 1);
    return out;
}

char* copy_string_view(std::string_view s)
{
    auto* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (out == nullptr) {
        return nullptr;
    }

    if (!s.empty()) {
        std::memcpy(out, s.data(), s.size());
    }
    out[s.size()] = '\0';
    return out;
}

} // namespace

extern "C" char* bdkffi_script_error_string(int code)
{
    try {
        if (code < 0 || code >= SCRIPT_ERR_ERROR_COUNT) {
            return copy_c_string("unknown-script-error");
        }
        return copy_c_string(ScriptErrorString(static_cast<ScriptError_t>(code)));
    } catch (...) {
        return copy_c_string("exception");
    }
}

extern "C" char* bdkffi_dos_error_string(int code)
{
    try {
        return copy_string_view(bsv::DoSErrorString(static_cast<bsv::DoSError_t>(code)));
    } catch (...) {
        return copy_c_string("exception");
    }
}

extern "C" int bdkffi_cpp_script_err_error_count(void)
{
    try {
        return bsv::CPP_SCRIPT_ERR_ERROR_COUNT();
    } catch (...) {
        return 0;
    }
}
