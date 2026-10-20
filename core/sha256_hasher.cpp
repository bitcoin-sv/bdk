// bitcoin-sv's CSHA256 (src/crypto/sha256.cpp) for the native core, compiled
// together with the load-time SHA-256 backend selection.
//
// Everything in BDK that hashes calls CSHA256, so any program or library that
// can hash links this object, and with it the initializer below. It selects the
// backend, reading BDK_SHA256_FORCE_SCALAR, while the program or library is
// loaded: before main(), before Go runs any init function, before any thread
// can hash. After that the sha256_dispatch pointers are only read.
//
// The wasm variant builds SV's sha256.cpp itself and stays scalar, so this is
// an empty translation unit there.

#if !defined(__EMSCRIPTEN__)

#include "crypto/sha256.cpp"

#include "sha256_backend.hpp"

namespace
{
    void SelectSha256BackendAtLoad() noexcept
    {
        try
        {
            bsv::SelectSha256Backend();
        }
        catch(...)
        {
            // Out of memory at load: the scalar transforms stay installed, and
            // the next SelectSha256Backend() call tries again
        }
    }

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
    [[gnu::constructor]] void RunSelectSha256BackendAtLoad() { SelectSha256BackendAtLoad(); }
#else
    const bool kSelectedAtLoad = (SelectSha256BackendAtLoad(), true);
#endif
}

#endif
