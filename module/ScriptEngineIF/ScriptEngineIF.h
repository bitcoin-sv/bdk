#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <string>
#include <memory>
#include <script_error.h>
#include <script/script_error.h>

namespace ScriptEngineIF
{
    ScriptError executeScript(bsv::span<const uint8_t>);
    bool verifyScript(const std::unique_ptr<unsigned char[]>&, const size_t& );

    ScriptError executeScript(const std::string& ) ;
    bool verifyScript(const std::string& );
};

#endif//#ifnde __SCRIPT_ENGINE_IF_H__
