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

    std::string Bin2Hex(const std::vector<uint8_t>& data) {
        //std::stringstream ss;
        //// Iterate over each byte in the vector and convert to hex
        //for (const auto& byte : data) {
        //    ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
        //}
        //return ss.str();
        constexpr std::array<char, 16> hexmap = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
        const size_t len = data.size();
        std::string hex; hex.reserve(2 * len);
        for (uint8_t byte : data) {
            hex.push_back(hexmap[(byte >> 4) & 0x0F]); // Upper nibble (4 bits)
            hex.push_back(hexmap[byte & 0x0F]);        // Lower nibble (4 bits)
        }
        return hex;
    }

}