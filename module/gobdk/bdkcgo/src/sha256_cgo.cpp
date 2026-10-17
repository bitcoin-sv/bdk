#include <bdkcgo/sha256_cgo.h>

#include "sha256_backend.hpp"

const char* cgo_sha256_backend(void)
{
    try
    {
        return bsv::SelectSha256Backend().c_str();
    }
    catch(...)
    {
        return "";
    }
}
