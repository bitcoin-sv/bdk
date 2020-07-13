//
// Created by m.fletcher on 10/07/2020.
//

#ifndef BSCRYPT_ASSEMBLER_H
#define BSCRYPT_ASSEMBLER_H

#include <core_io.h>
#include <script/script.h>

namespace bsv
{
    CScript from_asm(const std::string& script);
    std::string to_asm(const bsv::span<const uint8_t> script);
}

#endif // BSCRYPT_ASSEMBLER_H
