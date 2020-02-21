#ifndef FLAG_MAP_HPP
#define FLAG_MAP_HPP
///////////////////////////////////////////////////////////////////
//  Date             07/02/2020                                  //
//  Author           Chi Thanh NGUYEN                            //
//                                                               //
//  Copyright (c) 2020 nChain Limited. All rights reserved       //
///////////////////////////////////////////////////////////////////

// FlagMap contains all tools converting bitcoin flags enumarations to std::string
//
#include <boost/bimap.hpp>
#include <script.h>       // bitcoin code
#include <script_flags.h> // bitcoin code
#include <script_error.h> // bitcoin code

namespace bs
{
    ///! return the bimap opcode <--> string name
    boost::bimap< opcodetype, std::string > get_opcode_name_map();
    boost::bimap< opcodetype, std::string > get_opcode_shortname_map();// truncated "OP_", similar to native bitcoin json test

    ///! return the bimap script flags <--> string name
    boost::bimap< uint32_t, std::string > get_script_flag_name_map();
    boost::bimap< uint32_t, std::string > get_script_flag_shortname_map();// truncated "SCRIPT_VERIFY_", similar to native bitcoin json test

    ///! return the bimap script error <--> string name
    boost::bimap< ScriptError_t, std::string > get_script_error_name_map();
    boost::bimap< ScriptError_t, std::string > get_script_error_shortname_map();// truncated "SCRIPT_ERR_", similar to native bitcoin json test

    ///! from a list of flags (short) name separated by comma, return the combined flag
    uint32_t string2flag(const std::string& strFlags);
    uint32_t string2flag_short(const std::string& strFlags);
}

#endif /* FLAG_MAP_HPP */