#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <script/script_error.h>
#include <string>
#include <vector>

namespace bsv
{
    ScriptError execute(bsv::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags);

    ScriptError execute(bsv::span<const uint8_t> script,
                        bool consensus,
                        unsigned int flags,
                        bsv::span<const uint8_t> tx,
                        int _index,
                        int64_t amount);

    ScriptError execute(const std::string& script,
                        bool consensus,
                        unsigned int flags,
                        const std::string& tx,
                        int index,
                        int64_t amount);

    ScriptError verify(bsv::span<const uint8_t> unlocking_script,
                       bsv::span<const uint8_t> locking_script,
                       bool consensus,
                       unsigned int flags,
                       bsv::span<const uint8_t> tx,
                       int index,
                       int64_t amount);
};

#endif
