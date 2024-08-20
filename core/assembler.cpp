//
// Created by m.fletcher on 10/07/2020.
//
#include "assembler.h"

#include "core_io.h"
#include "script.h"

namespace bsv {

    CScript from_asm(const std::string& script)
    {
        return ParseScript(script);
    }

    std::string to_asm(const std::span<const uint8_t> script)
    {
        return FormatScript(CScript(script.data(), script.data() + script.size()));
    }

}