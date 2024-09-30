#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include <span>

#include <script/script_error.h>
#include <string>
#include <vector>
#include <stdint.h>

namespace bsv
{

    // script_verification_flags calculates the flags to be used when verifying scripts
    uint32_t script_verification_flags(const std::span<const uint8_t> locking_script, const bool isPostChronical);
    uint32_t script_verification_flags_v2(const std::span<const uint8_t> locking_script, int32_t blockHeight);

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
