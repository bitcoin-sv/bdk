#include <script/script_error.h>
#include <bdkcgo/script_error_cgo.h>

const char *cgo_script_error_string(const int e) {
    // BDK-specific error codes for standardness failures
    if (e == 200) return "non-standard transaction";
    if (e == 201) return "non-standard input";

    if (e > SCRIPT_ERR_ERROR_COUNT) {
        return "CGO EXCEPTION : Exception has been thrown and handled in C/C++ layer";
    }

    return ScriptErrorString(ScriptError_t(e));
}