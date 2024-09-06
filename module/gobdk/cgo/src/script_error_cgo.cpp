#include <script/script_error.h>
#include <cgo/script_error_cgo.h>

const char *cgo_script_error_string(const int e) {
    if (e > SCRIPT_ERR_ERROR_COUNT) {
        return "CGO EXCEPTION : Exception has been thrown and handled in C/C++ layer";
    }

    return ScriptErrorString(ScriptError_t(e));
}