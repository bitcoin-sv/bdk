#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindBoostHelper)
HelpFindBoost()

if (NOT (Boost_FOUND AND OPENSSL_FOUND))
  message(FATAL_ERROR "Boost and OpenSSL are required to build bdk_core")
endif()

## Build bdk requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

#### Generate version C++  #####
set(BDK_VERSION_CPP_IN ${CMAKE_CURRENT_SOURCE_DIR}/BDKVersion.cpp.in CACHE INTERNAL "Template File for C++ version")
set(BDK_VERSION_CPP ${BDK_GENERATED_CPP_DIR}/BDKVersion.cpp CACHE INTERNAL "CPP File for framework version config")
configure_file(${BDK_VERSION_CPP_IN} ${BDK_VERSION_CPP})

set(BSV_MINIMAL_FILES ${BSV_MINIMAL_HDR_FILES} ${BSV_MINIMAL_SRC_FILES})
#bdkPrintList("BSV_MINIMAL_FILES" "${BSV_MINIMAL_FILES}")## Help to debug

## Helper files activate some settings for bsv and secp256k1 native source codes
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-bitcoin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-secp256k1.cmake")
if(BDK_BUILD_UNIVALUE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-univalue.cmake")
  set(UNIVALUE_LIB univalue)
else()
  set(UNIVALUE_LIB "")
endif()
if(BDK_BUILD_LEVELDB)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-leveldb.cmake")
endif()

## Log list of bsv source files
message(STATUS "Build core using bsv source code in ${BDK_BSV_ROOT_DIR}")
foreach(_bitcoin_file ${BSV_MINIMAL_FILES})
  if(BDK_LOG_BSV_FILES)
    message(STATUS "    [${_bitcoin_file}]")
  endif()
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_bitcoin_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${BDK_BSV_ROOT_DIR} PREFIX "bitcoin SRC" FILES "${_bitcoin_file}")
  else()
    source_group(TREE ${BDK_BSV_ROOT_DIR} PREFIX "bitcoin HDR" FILES "${_bitcoin_file}")
  endif()
endforeach()
source_group("_generated" FILES "${BITCOIN_CONFIG_FILE}" "${BDK_VERSION_CPP}")

## any .cpp .hpp file added here will be added group with bsv file to build the core.
file(GLOB_RECURSE BDK_CORE_EXTRA_HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE BDK_CORE_EXTRA_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(BDK_CORE_HDR_FILES ${BDK_CORE_EXTRA_HDR_FILES} ${BSV_MINIMAL_HDR_FILES})
set(BDK_CORE_SRC_FILES ${BDK_CORE_EXTRA_SRC_FILES} ${BSV_MINIMAL_SRC_FILES})
message(STATUS "Extra c++ file in ${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_extra_file ${BDK_CORE_EXTRA_HDR_FILES} ${BDK_CORE_EXTRA_SRC_FILES})
  if(BDK_LOG_BSV_FILES)
    message(STATUS "      +[${_extra_file}]")
  endif()
endforeach()

## Build bdk_core as a library using bsv source code
add_library(bdk_core ${BITCOIN_CONFIG_FILE} ${BDK_VERSION_CPP} ${BDK_CORE_HDR_FILES} ${BDK_CORE_SRC_FILES})
#set_target_properties(bdk_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
target_include_directories(bdk_core PUBLIC ${BSV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bdk_core PRIVATE secp256k1 ${UNIVALUE_LIB} OpenSSL::Crypto OpenSSL::SSL)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # remove warning on linux
    # Add the warning suppression flag to a specific source file
    set_source_files_properties("${BDK_BSV_ROOT_DIR}/src/script/sign.cpp" PROPERTIES COMPILE_FLAGS "-Wno-stringop-overread")
endif()

if(WIN32)
  target_link_libraries(bdk_core PRIVATE Crypt32.lib Ws2_32)
endif()
target_compile_definitions(bdk_core PUBLIC HAVE_CONFIG_H BOOST_THREAD_USE_LIB BOOST_SP_USE_STD_ATOMIC BOOST_AC_USE_STD_ATOMIC)
bdkLinkTargetToBoost(bdk_core Boost::boost)

set_property(TARGET bdk_core PROPERTY FOLDER "core")

## Install the binary library
install(TARGETS bdk_core DESTINATION "lib" COMPONENT bdk_core)

## Install bitcoin config file
install(FILES ${BITCOIN_CONFIG_FILE} DESTINATION "include/config" COMPONENT bdk_core)
 
## Install bdk_core : the *.h header files should be kept as bsv structure
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include files in bsv\n")
## Calculate the relative path for include file in installer
get_filename_component(__f ${BITCOIN_CONFIG_FILE} NAME)
set(__include_rl "config/${__f}")
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${__include_rl}\"\n")
set(default_install_include_core_dir "include")
# Install bsv header files
foreach(_bsv_hdr ${BSV_MINIMAL_HDR_FILES})
  set(_relative_dir "")#clear
  get_filename_component(_dir ${_bsv_hdr} DIRECTORY)
  if("${_dir}" STRGREATER "${BDK_BSV_ROOT_DIR}/src")
    string(REPLACE "${BDK_BSV_ROOT_DIR}/src/" "" _relative_dir "${_dir}")
    set(_install_dest "${default_install_include_core_dir}/${_relative_dir}")
  else()
    set(_install_dest "${default_install_include_core_dir}")
  endif()
  #message("install file [${_bsv_hdr}]  _install_dest[${_install_dest}]")
  install(FILES ${_bsv_hdr} DESTINATION "${_install_dest}" COMPONENT bdk_core)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_bsv_hdr} NAME)
  set(_include_rl "${_f}")
  if(NOT "${_relative_dir}" STREQUAL "")
    set(_include_rl "${_relative_dir}/${_f}")
  endif()
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_include_rl}\"\n")
endforeach()
# Install extra core header files
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include extra BDK files in core\n")
foreach(_extra_hdr ${BDK_CORE_EXTRA_HDR_FILES})
  install(FILES ${_extra_hdr} DESTINATION "${default_install_include_core_dir}" COMPONENT bdk_core)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_extra_hdr} NAME)
  set(_include_rl "core/${_f}")
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_include_rl}\"\n")
endforeach()

set(BDK_UNIFIED_HEADER_FILE "${BDK_GENERATED_TOOLS_DIR}/bdk")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/bdk.in" ${BDK_UNIFIED_HEADER_FILE})
install(FILES ${BDK_UNIFIED_HEADER_FILE} DESTINATION "include/core" COMPONENT bdk_core)