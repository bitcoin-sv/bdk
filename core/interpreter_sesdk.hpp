#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include <span>

#include <script/script_error.h>
#include <string>
#include <vector>
#include <cstdint>

namespace bsv
{

    // script_verification_flags calculates the flags to be used when verifying scripts
    // It is calculated based on the locking script and the boolean isPostChronical
    // If the node parameter -genesis is set to true, then the argument isPostChronical is false
    // Otherwise, isPostChronical is true
    unsigned int script_verification_flags(const std::span<const uint8_t> locking_script, const bool isPostChronical);

    ScriptError execute(std::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(std::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags,
                        std::span<const uint8_t> tx,
                        int _index,
                        int64_t amount);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags,
                        const std::string& tx,
                        int index,
                        int64_t amount);

    ScriptError verify(std::span<const uint8_t> unlocking_script,
                       std::span<const uint8_t> locking_script,
                       bool consensus,
                       unsigned int flags,
                       std::span<const uint8_t> tx,
                       int index,
                       int64_t amount);
};

#endif
