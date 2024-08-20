#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build scrypt requires locating bsv source code
if(NOT DEFINED SESDK_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by SESDK_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${SESDK_BSV_ROOT_DIR}/sv/src/config/CMakeLists.txt with slightl modifications
#######################################################################




# Copyright (c) 2017 The Bitcoin developers
# Copyright (c) 2019 Bitcoin Association
# Distributed under the Open BSV software license, see the accompanying file LICENSE.

# This generates config.h which provides numerous defines
# about the state of the plateform we are building on.

include(CheckIncludeFiles)
include(CheckSymbolExists)

# Package informations
set(PACKAGE_NAME "Bitcoin SV")

# Version
set(CLIENT_VERSION_MAJOR 1)
set(CLIENT_VERSION_MINOR 0)
set(CLIENT_VERSION_REVISION 0)
set(CLIENT_VERSION_BUILD 0)

if(ENABLE_PROD_BUILD)
    set(CLIENT_VERSION_IS_RELEASE "true")
else()
    set(CLIENT_VERSION_IS_RELEASE "false")
endif()


# Copyright
set(COPYRIGHT_YEAR 2019)
set(COPYRIGHT_HOLDERS "The %s developers")
set(COPYRIGHT_HOLDERS_SUBSTITUTION Bitcoin)
string(REPLACE "%s" ${COPYRIGHT_HOLDERS_SUBSTITUTION} COPYRIGHT_HOLDERS_FINAL ${COPYRIGHT_HOLDERS})

# Endianness
check_include_files("endian.h" HAVE_ENDIAN_H)
check_include_files("sys/endian.h" HAVE_SYS_ENDIAN_H)

if(HAVE_ENDIAN_H)
    set(ENDIAN_FILE "endian.h")
elseif(HAVE_SYS_ENDIAN_H)
    set(ENDIAN_FILE "sys/endian.h")
else()
endif()

if(ENDIAN_FILE)
    check_symbol_exists(htole16 ${ENDIAN_FILE} HAVE_DECL_HTOLE16)
    check_symbol_exists(htobe16 ${ENDIAN_FILE} HAVE_DECL_HTOBE16)
    check_symbol_exists(be16toh ${ENDIAN_FILE} HAVE_DECL_BE16TOH)
    check_symbol_exists(le16toh ${ENDIAN_FILE} HAVE_DECL_LE16TOH)
    check_symbol_exists(htobe32 ${ENDIAN_FILE} HAVE_DECL_HTOBE32)
    check_symbol_exists(htole32 ${ENDIAN_FILE} HAVE_DECL_HTOLE32)
    check_symbol_exists(be32toh ${ENDIAN_FILE} HAVE_DECL_BE32TOH)
    check_symbol_exists(le32toh ${ENDIAN_FILE} HAVE_DECL_LE32TOH)
    check_symbol_exists(htobe64 ${ENDIAN_FILE} HAVE_DECL_HTOBE64)
    check_symbol_exists(htole64 ${ENDIAN_FILE} HAVE_DECL_HTOLE64)
    check_symbol_exists(be64toh ${ENDIAN_FILE} HAVE_DECL_BE64TOH)
    check_symbol_exists(le64toh ${ENDIAN_FILE} HAVE_DECL_LE64TOH)
endif()

# Byte swap
check_include_files("byteswap.h" HAVE_BYTESWAP_H)

check_symbol_exists(bswap_16 "byteswap.h" HAVE_DECL_BSWAP_16)
check_symbol_exists(bswap_32 "byteswap.h" HAVE_DECL_BSWAP_32)
check_symbol_exists(bswap_64 "byteswap.h" HAVE_DECL_BSWAP_64)

# Bitmanip intrinsics
function(check_builtin_exist SYMBOL VARIABLE)
    set(
        SOURCE_FILE
        "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckBuiltinExists.c"
    )
    set(
        CMAKE_CONFIGURABLE_FILE_CONTENT
        "int main(int argc, char** argv) { (void)argv; return ${SYMBOL}(argc); }\n"
    )
    configure_file(
        "${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in"
        "${SOURCE_FILE}"
        @ONLY
    )
    if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for ${SYMBOL}")
    endif()
    try_compile(${VARIABLE}
        ${CMAKE_BINARY_DIR}
        ${SOURCE_FILE}
        OUTPUT_VARIABLE OUTPUT
    )
    if(${VARIABLE})
        if(NOT CMAKE_REQUIRED_QUIET)
            message(STATUS "Looking for ${SYMBOL} - found")
        endif()
        set(${VARIABLE} 1 CACHE INTERNAL "Have symbol ${SYMBOL}")
        file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
            "Determining if the ${SYMBOL} "
            "exist passed with the following output:\n"
            "${OUTPUT}\nFile ${SOURCEFILE}:\n"
            "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n")
    else()
        if(NOT CMAKE_REQUIRED_QUIET)
            message(STATUS "Looking for ${SYMBOL} - not found")
        endif()
        set(${VARIABLE} "" CACHE INTERNAL "Have symbol ${SYMBOL}")
        file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
            "Determining if the ${SYMBOL} "
            "exist failed with the following output:\n"
            "${OUTPUT}\nFile ${SOURCEFILE}:\n"
            "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n")
    endif()
endfunction()

check_builtin_exist(__builtin_clz HAVE_DECL___BUILTIN_CLZ)
check_builtin_exist(__builtin_clzl HAVE_DECL___BUILTIN_CLZL)
check_builtin_exist(__builtin_clzll HAVE_DECL___BUILTIN_CLZLL)

# Various system libraries
check_symbol_exists(strnlen "string.h" HAVE_DECL_STRNLEN)
check_symbol_exists(daemon "unistd.h" HAVE_DECL_DAEMON)

# OpenSSL functionality
#include(BrewHelper)
#find_brew_prefix(OPENSSL_ROOT_DIR openssl)
#find_package(OpenSSL REQUIRED)
#set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_CRYPTO_INCLUDES})
#set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY})
#check_symbol_exists(EVP_MD_CTX_new "openssl/evp.h" HAVE_DECL_EVP_MD_CTX_NEW)

# Activate wallet
#set(ENABLE_WALLET ${BUILD_BITCOIN_WALLET})

# Activate ZeroMQ
#set(ENABLE_ZMQ ${BUILD_BITCOIN_ZMQ})

# Generate the config
set(BITCOIN_CONFIG_IN "${SESDK_BSV_ROOT_DIR}/src/config/bitcoin-config.h.cmake.in")
set(BITCOIN_CONFIG_FILE "${SESDK_GENERATED_HPP_DIR}/config/bitcoin-config.h")
configure_file("${BITCOIN_CONFIG_IN}" "${BITCOIN_CONFIG_FILE}" ESCAPE_QUOTES)


message(STATUS "SESDK Warning : Hot fix for Visual Studio 17 2022 and gcc 13")
message(STATUS "SESDK Warning : The build fails because in file src/script/script.h, it use std::array but doesn't include <array>")
message(STATUS "SESDK Warning : We \"hot-fix\" that by make this include <array> in the generated file config/bitcoin-config.h")
message(STATUS "SESDK Warning : which is in our control")

# Read the file content and prepare lines to replace
file(READ "${BITCOIN_CONFIG_FILE}" FILE_CONTENTS)
set(LINE_TO_INSERT "\n\n#include <array>\n#include <stdexcept>\n#include <cstdint>\n#include <ranges>\n#include <span>\n")
set(INSERT_AFTER "#define BITCOIN_CONFIG_BITCOIN_CONFIG_H")

# Modify the content: Insert the new line after a specific line
string(REPLACE "${INSERT_AFTER}" "${INSERT_AFTER}${LINE_TO_INSERT}" MODIFIED_CONTENTS "${FILE_CONTENTS}")

# Write the modified content back to the file
file(WRITE "${BITCOIN_CONFIG_FILE}" "${MODIFIED_CONTENTS}")
