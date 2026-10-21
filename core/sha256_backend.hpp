#pragma once

#include <string>
#include <vector>

#include "crypto/sha256_dispatch.h"

namespace bsv
{

/**
 * One set of SHA-256 transforms this CPU can run.
 *   name         : "standard", "sse4", "shani" or "arm_shani"
 *   stream       : candidate for sha256_dispatch::stream
 *   d_block_2way : candidate for sha256_dispatch::d_block_2way, nullptr if the backend has none
 */
struct Sha256Backend
{
    const char* name;
    sha256_dispatch::StreamType stream;
    sha256_dispatch::DBlock2wayType d_block_2way;
};

/**
 * Every backend built into this library that the running CPU supports and that
 * passed its self-test, "standard" (scalar) first and the fastest last.
 */
const std::vector<Sha256Backend>& Sha256SupportedBackends();

/**
 * Installs the fastest supported transforms into the sha256_dispatch pointers
 * and returns a description of what was installed, e.g. "shani,shani(2way)" or
 * "arm_shani,arm_shani(2way)". It does the work once; later calls return the
 * same string and write nothing.
 *
 * BDK calls it while it is loaded, from the object that holds CSHA256
 * (core/sha256_hasher.cpp), so the selection is made before main(), before Go
 * runs any init function and before any thread can hash. Every CTxValidator
 * constructor calls it as well; after load that is a no-op.
 *
 * BDK_SHA256_FORCE_SCALAR is read at that point, at load. Set to anything other
 * than "" or "0" in the environment the process starts with, it keeps the
 * scalar transforms and the result is "standard(forced)". Setting it later, for
 * example with Go's os.Setenv, has no effect. The WASM build is always
 * "standard".
 *
 * Do not call sha256_dispatch::AutoDetect() directly. On x86-64 it installs
 * SSE4 / SHA-NI regardless of BDK_SHA256_FORCE_SCALAR: under forced scalar BDK
 * never calls it, so a direct call is the first write to the dispatch pointers,
 * and this function still returns "standard(forced)". Called from another
 * static initializer before BDK's selection, it writes the pointers outside
 * this function's once-only guard. On ARM64 it installs nothing. Call
 * SelectSha256Backend() instead.
 */
const std::string& SelectSha256Backend();

} // namespace bsv
