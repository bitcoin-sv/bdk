#include <bdkffi/alloc.h>

#include <cstdlib>

extern "C" void bdkffi_free(void* p)
{
    try {
        std::free(p);
    } catch (...) {
    }
}
