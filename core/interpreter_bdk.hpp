#ifndef __INTERPRETER_BDK_HPP__
#define __INTERPRETER_BDK_HPP__



#include <script/script_error.h>
#include <script/malleability_status.h>

#include <span>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <variant>

namespace bsv
{
    // This helper function transforms the result of BSV::EvalScript to a raw ScriptError
    // This method is not used in verify script, but to use after calling bsv::execute
    ScriptError get_raw_eval_script(const std::optional<std::variant<ScriptError, malleability::status>>& hybridResult);

    // This helper function transforms the result of BSV::VerifyScript to a raw ScriptError
    ScriptError get_raw_verify_script(const std::optional<std::pair<bool, ScriptError>>& ret);

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

#endif /* __INTERPRETER_BDK_HPP__ */
