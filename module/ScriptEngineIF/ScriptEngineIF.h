#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <string>

namespace ScriptEngineIF
{
    bool executeScript(bsv::span<const uint8_t>);
    bool executeScript(const std::string&) ; 
    
    bool verifyScript(bsv::span<const uint8_t>); 
    bool verifyScript(const std::string&);
};

#endif//#ifnde __SCRIPT_ENGINE_IF_H__
