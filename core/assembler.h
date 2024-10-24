#ifndef __ASSEMBLER_HPP__
#define __ASSEMBLER_HPP__

//
// Created by m.fletcher on 10/07/2020.
//

#include <string>
#include <cstdint>
#include <span>
#include <vector>

class CScript;

namespace bsv
{
    // Convert string script ( list of op_code string) to a script
    CScript from_asm(const std::string& script);

    // Convert script to string script ( list of op_code string)
    std::string to_asm(const std::span<const uint8_t> script);

    // Convert a binary to hex string
    std::string Bin2Hex(const std::vector<uint8_t>& data);
}

#endif /* __ASSEMBLER_HPP__ */