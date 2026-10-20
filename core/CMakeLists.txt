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

## Canonical native core: OpenSSL-linked, no exclusions, no extra definitions
## (OpenSSL discovery runs at root scope, visible to core/, module/ and test/)
bdk_add_core_library(bdk_core
  LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)

## Hardware SHA-256, native core only (the wasm variant stays scalar).
## sha256_dispatch.cpp defines sha256_dispatch::AutoDetect(), declared in the
## installed crypto/sha256_dispatch.h. The x86-64 sources below are built for
## every target: each guards itself with the compiler's own target macros, so
## a macOS universal, Rosetta or cross build gets them in the x86-64 code only.
## No file gets instruction-set flags: core/sha256_x86_shani.cpp (SV's
## sha256d64_shani.cpp) and core/sha256_arm_shani_target.cpp enable SHA-NI and
## the ARMv8 SHA-2 instructions with target pragmas around the included code.
## bsv::SelectSha256Backend() (sha256_backend.cpp) picks at run time.
## The source properties are directory-scoped, so they never reach the wasm
## variant, which builds the same core/ files from its own directory.
set(_bsv_crypto_dir "${BDK_BSV_ROOT_DIR}/src/crypto")
target_sources(bdk_core PRIVATE
  "${_bsv_crypto_dir}/sha256_dispatch.cpp"
  "${_bsv_crypto_dir}/sha256_sse4.cpp"
  "${_bsv_crypto_dir}/sha256_sse4_shim.cpp"
  "${_bsv_crypto_dir}/sha256_shani_shim.cpp"
  "${_bsv_crypto_dir}/sha256d64_shani_shim.cpp")
## SV defines USE_ASM per target rather than in bitcoin-config.h; without it
## AutoDetect() silently skips SSE4. Only read under __x86_64__.
set_source_files_properties("${_bsv_crypto_dir}/sha256_dispatch.cpp" PROPERTIES
  COMPILE_DEFINITIONS "USE_ASM")

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
