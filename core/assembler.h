#pragma once

//
// Created by m.fletcher on 10/07/2020.
//

#include <cstdint>
#include <span>
#include <string>
#include <vector>

class CScript;

namespace bsv
{
    // Convert string script ( list of op_code string) to a script
    CScript from_asm(const std::string& script);

    // Convert script to string script ( list of op_code string)
    std::string to_asm(std::span<const uint8_t> script);

    // Convert a binary to hex string
    std::string Bin2Hex(const std::vector<uint8_t>& data);
}
