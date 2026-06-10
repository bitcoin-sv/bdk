#include <bdkffi/error_strings.h>

#include <core/doserror.hpp>
#include <core/txvalidator.hpp>
#include <script/script_error.h>

extern "C" const char* bdkffi_script_error_string(int code)
{
    try {
        if (code < 0 || code > SCRIPT_ERR_ERROR_COUNT) {
            return "unknown-script-error";
        }
        return ScriptErrorString(static_cast<ScriptError_t>(code));
    } catch (...) {
        return "exception";
    }
}

extern "C" const char* bdkffi_dos_error_string(int code)
{
    try {
        // DoSErrorString currently returns string_views over static,
        // null-terminated string literals. Returning data() depends on that
        // invariant remaining true.
        return bsv::DoSErrorString(static_cast<bsv::DoSError_t>(code)).data();
    } catch (...) {
        return "exception";
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
