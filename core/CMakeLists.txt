#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindBoostHelper)
HelpFindBoost()

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost is required to build bdk_core")
endif()

## Build bdk requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

## The reusable core recipe: setting-*.cmake includes plus the
## bdk_add_core_library() factory building one core library variant
include("${CMAKE_CURRENT_SOURCE_DIR}/bdk-core-recipe.cmake")

## LevelDB is a native-only side build (no core variant links it), so it stays
## here in the thin native part, outside the reusable recipe
if(BDK_BUILD_LEVELDB)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-leveldb.cmake")
endif()

## TRANSITIONAL: the root still loads cmake/BDKCoreConfig.cmake, so the profile
## variables keep feeding the canonical core here. The root-wiring commit
## removes that profile mechanism and replaces this call with the explicit
## canonical arguments (LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL).
## (OpenSSL discovery runs at root scope, visible to core/, module/ and test/)
bdk_add_core_library(bdk_core
  EXCLUDE_BSV_SOURCES ${BDK_CORE_EXCLUDED_BSV_SOURCES}
  ADDITIONAL_SOURCES ${BDK_CORE_ADDITIONAL_SOURCES}
  LINK_LIBRARIES ${BDK_CORE_LINK_LIBRARIES}
  COMPILE_DEFINITIONS ${BDK_CORE_PRIVATE_COMPILE_DEFINITIONS})

## Install the binary library
if(BDK_INSTALL_CORE_ARCHIVE)
  install(TARGETS bdk_core DESTINATION "lib" COMPONENT bdk_core)
endif()

if(BDK_INSTALL_BSV_HEADERS)
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
endif()
