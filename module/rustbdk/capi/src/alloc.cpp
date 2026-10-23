#include <bdkffi/alloc.h>

#include <cstdlib>

extern "C" void bdkffi_free(void* p)
{
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
        std::free(p);
    } catch (...) {
    }
}
