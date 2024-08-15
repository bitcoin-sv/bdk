//
// Created by m.fletcher on 10/07/2020.
//
#pragma once

#include <string>
#include <cstdint>

#include "span.h"

class CScript;

namespace bsv
{
    CScript from_asm(const std::string& script);

    std::string to_asm(const bsv::span<const uint8_t> script);
}

