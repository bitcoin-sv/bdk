#include <bdkcgo/doserror_cgo.h>
#include <core/doserror.hpp>

const char* dos_error_string(int error) {
    return bsv::DoSErrorString(static_cast<bsv::DoSError_t>(error)).data();
}
