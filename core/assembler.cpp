//
// Created by m.fletcher on 10/07/2020.
//
#include "assembler.h"

#include "core_io.h"
#include "script.h"

CScript bsv::from_asm(const std::string& script)
{
    return ParseScript(script);
}

std::string bsv::to_asm(const std::string& script)
{
    if(script.empty() ||
       script.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        throw std::runtime_error(
            "No script provided to formatScript in ScriptEngine::formatScript");
    }

    CScript cscript{ParseScript(script)};
    return FormatScript(cscript);
}

std::string bsv::to_asm(const bsv::span<const uint8_t> script)
{
    return FormatScript(CScript{script.begin(), script.end()});
}
