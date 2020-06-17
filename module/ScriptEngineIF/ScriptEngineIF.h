#ifndef __SCRIPT_ENGINE_IF_H__
#define __SCRIPT_ENGINE_IF_H__

#include "span.h"

#include <string>
#include <memory>

namespace ScriptEngineIF
{
    bool executeScript(bsv::span<const uint8_t>);
    bool verifyScript(const std::unique_ptr<unsigned char[]>&, const size_t& ); 
    
    bool executeScript(const std::string& ) ; 
    bool verifyScript(const std::string& );
};

#endif//#ifnde __SCRIPT_ENGINE_IF_H__
