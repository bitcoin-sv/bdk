#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <string>
#include <script_error.h>
#include <script/script_error.h>

namespace ScriptEngineIF
{
    ScriptError executeScript(bsv::span<const uint8_t>);
    bool executeScript(const std::string&) ;

    ScriptError verifyScript(bsv::span<const uint8_t>);
    bool verifyScript(const std::string&);
};

#endif//#ifnde __SCRIPT_ENGINE_IF_H__
