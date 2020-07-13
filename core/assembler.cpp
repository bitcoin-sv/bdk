//
// Created by m.fletcher on 10/07/2020.
//
#include "assembler.h"

CScript bsv::from_asm(const std::string& script)
{
    return ParseScript(script);
}

std::string bsv::to_asm(const bsv::span<const uint8_t> script)
{
    return FormatScript(CScript{script.begin(), script.end()});
}