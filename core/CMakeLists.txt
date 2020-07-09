#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if (NOT (Boost_FOUND AND OPENSSL_FOUND))
  message(FATAL_ERROR "Boost and OpenSSL are required to build scrypt_core")
endif()

## Build scrypt requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_ROOT_DIR")
endif()
set(BSV_MINIMAL_FILES ${BSV_MINIMAL_HDR_FILES} ${BSV_MINIMAL_SRC_FILES})
set(UNIFIED_INSTALL_HEADERS_STR "\n#include \"core/ScryptVersion.hpp\"\n\n")
#scryptPrintList("BSV_MINIMAL_FILES" "${BSV_MINIMAL_FILES}")## Help to debug

## Helper files activate some settings for bsv and secp256k1 native source codes
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-bitcoin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-secp256k1.cmake")
if(SCRYPT_BUILD_UNIVALUE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-univalue.cmake")
  set(UNIVALUE_LIB univalue)
else()
  set(UNIVALUE_LIB "")
endif()
if(SCRYPT_BUILD_LEVELDB)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-leveldb.cmake")
endif()

## Log list of bsv source files
message(STATUS "Build core using bsv source code in ${SCRYPT_BSV_ROOT_DIR}")
foreach(_bitcoin_file ${BSV_MINIMAL_FILES})
  if(SCRYPT_LOG_BSV_FILES)
    message(STATUS "    [${_bitcoin_file}]")
  endif()
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_bitcoin_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${SCRYPT_BSV_ROOT_DIR} PREFIX "bitcoin SRC" FILES "${_bitcoin_file}")
  else()
    source_group(TREE ${SCRYPT_BSV_ROOT_DIR} PREFIX "bitcoin HDR" FILES "${_bitcoin_file}")
  endif()
endforeach()
source_group("_generated" FILES "${BITCOIN_CONFIG_FILE}")

## any .cpp .hpp file added here will be added group with bsv file to build the core.
file(GLOB_RECURSE BSCRYPT_CORE_EXTRA_HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE BSCRYPT_CORE_EXTRA_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(BSCRYPT_CORE_HDR_FILES ${BSCRYPT_CORE_EXTRA_HDR_FILES} ${BSV_MINIMAL_HDR_FILES})
set(BSCRYPT_CORE_SRC_FILES ${BSCRYPT_CORE_EXTRA_SRC_FILES} ${BSV_MINIMAL_SRC_FILES})
message(STATUS "Extra c++ file in ${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_extra_file ${BSCRYPT_CORE_EXTRA_HDR_FILES} ${BSCRYPT_CORE_EXTRA_SRC_FILES})
  if(SCRYPT_LOG_BSV_FILES)
    message(STATUS "      +[${_extra_file}]")
  endif()
endforeach()

## Build scrypt_core as a library using bsv source code
add_library(scrypt_core ${BITCOIN_CONFIG_FILE} ${BSCRYPT_CORE_HDR_FILES} ${BSCRYPT_CORE_SRC_FILES})
#set_target_properties(scrypt_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
target_include_directories(scrypt_core PUBLIC ${BSV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrypt_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 ${UNIVALUE_LIB} OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)

if(WIN32)
  target_link_libraries(scrypt_core Crypt32.lib Ws2_32)
endif()
target_compile_definitions(scrypt_core PUBLIC HAVE_CONFIG_H BOOST_THREAD_USE_LIB BOOST_SP_USE_STD_ATOMIC BOOST_AC_USE_STD_ATOMIC)
scryptLinkTargetToBoost(scrypt_core Boost::boost)

set_property(TARGET scrypt_core PROPERTY FOLDER "core")

## Install the binary library
install(TARGETS scrypt_core DESTINATION "lib" COMPONENT scrypt_core)

set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include files in bsv\n")
## Install scrypt_core : the *.h header files should be kept as bsv structure
install(FILES ${BITCOIN_CONFIG_FILE} DESTINATION "include/core/config" COMPONENT scrypt_core)
 
## Calculate the relative path for include file in installer
get_filename_component(__f ${BITCOIN_CONFIG_FILE} NAME)
set(__install_rl "core/config/${__f}")
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${__install_rl}\"\n")
set(default_install_include_core_dir "include/core")
# Install bsv header files
foreach(_bsv_hdr ${BSV_MINIMAL_HDR_FILES})
  get_filename_component(_dir ${_bsv_hdr} DIRECTORY)
  if("${_dir}" STRGREATER "${SCRYPT_BSV_ROOT_DIR}/src")
    string(REPLACE "${SCRYPT_BSV_ROOT_DIR}/src" "" _relative_dir "${_dir}")
    set(_install_dest "${default_install_include_core_dir}${_relative_dir}")
  else()
    set(_install_dest "${default_install_include_core_dir}")
  endif()
  #message("install file [${_bsv_hdr}]  _install_dest[${_install_dest}]")
  install(FILES ${_bsv_hdr} DESTINATION "${_install_dest}" COMPONENT scrypt_core)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_bsv_hdr} NAME)
  set(_install_rl "core${_relative_dir}/${_f}")
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_install_rl}\"\n")
endforeach()
# Install extra core header files
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include extra bscrypt files in core\n")
foreach(_extra_hdr ${BSCRYPT_CORE_EXTRA_HDR_FILES})
  install(FILES ${_extra_hdr} DESTINATION "${default_install_include_core_dir}" COMPONENT scrypt_core)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_extra_hdr} NAME)
  set(_install_rl "core/${_f}")
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_install_rl}\"\n")
endforeach()

set(BSCRYPT_UNIFIED_HEADER_FILE "${SCRYPT_GENERATED_TOOLS_DIR}/script_engine")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/script_engine.in" ${BSCRYPT_UNIFIED_HEADER_FILE})
install(FILES ${BSCRYPT_UNIFIED_HEADER_FILE} DESTINATION "include/core" COMPONENT scrypt_core)
