#ifndef __SCRIPT_ENGINNE_IF_H__
#define __SCRIPT_ENGINNE_IF_H__


#include <string>
#include <memory>

namespace ScriptEngineIF{

    bool executeScript(const std::unique_ptr<unsigned char[]>&, const size_t& );
    bool verifyScript(const std::unique_ptr<unsigned char[]>&, const size_t& ); 
    
    bool executeScript(const std::string& ) ; 
    bool verifyScript(const std::string& );

};

#endif//#ifnde __SCRIPT_ENGINNE_IF_H__
