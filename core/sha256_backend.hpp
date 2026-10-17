#ifndef __SHA256_BACKEND_HPP__
#define __SHA256_BACKEND_HPP__

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
 * "arm_shani,arm_shani(2way)". It does the work once, on the first call; later
 * calls return the same string. Every CTxValidator constructor calls it first.
 *
 * Setting the environment variable BDK_SHA256_FORCE_SCALAR to anything other
 * than "" or "0" before the first call keeps the scalar transforms, and the
 * result is "standard(forced)". The WASM build is always "standard".
 *
 * A program that hashes on several threads before it constructs any
 * CTxValidator, for example while building CChainParams to pass their
 * Consensus::Params to one, should call this once at startup.
 *
 * Do not call sha256_dispatch::AutoDetect() directly: it writes the dispatch
 * pointers again, and on x86-64 it ignores BDK_SHA256_FORCE_SCALAR.
 */
const std::string& SelectSha256Backend();

} // namespace bsv

#endif /* __SHA256_BACKEND_HPP__ */
