#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <script/script_error.h>
#include <string>

namespace bsv
{
    ScriptError evaluate(bsv::span<const uint8_t> script,
                         bool consensus,
                         unsigned int flags,
                         const std::string& transaction,
                         int tx_input_index,
                         int64_t amount);
    ScriptError evaluate(const std::string& script,
                         bool consensus,
                         unsigned int flags,
                         const std::string& transaction,
                         int tx_input_index,
                         int64_t amount);

    std::string formatScript(const std::string& script);
};

#endif
