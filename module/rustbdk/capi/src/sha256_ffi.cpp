#include <bdkffi/sha256.h>

#include "sha256_backend.hpp"

extern "C" const char* bdkffi_sha256_backend(void)
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
