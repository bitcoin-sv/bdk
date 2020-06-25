#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <string>
#include <script_error.h>
#include <script/script_error.h>

namespace ScriptEngineIF
{
    ScriptError executeScript(bsv::span<const uint8_t>,const bool&, const unsigned int&, const std::string&, const int&, const int64_t&);
    ScriptError executeScript(const std::string&, const bool&, const unsigned int&, const std::string&, const int&, const int64_t&) ;

    bool verifyScript(bsv::span<const uint8_t>);
    ScriptError verifyScript(const std::string&,const std::string&, const bool&, const unsigned int&, const std::string&, const int&, const int64_t&);
    
     std::string formatScript(const std::string&);
};

#endif//#ifnde __SCRIPT_ENGINE_IF_H__
