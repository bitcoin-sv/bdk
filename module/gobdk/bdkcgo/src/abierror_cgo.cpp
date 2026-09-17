#include <bdkcgo/abierror_cgo.h>
#include <core/abierror.h>

const char* abi_error_string(int error) {
    return bsv::AbiErrorString(static_cast<bsv::AbiError_t>(error)).data();
}
