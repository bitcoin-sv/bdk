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
if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)#
  message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_SOURCE_ROOT")
endif()
set(BSV_MINIMAL_FILES ${BSV_MINIMAL_HDR_FILES} ${BSV_MINIMAL_SRC_FILES})
#scryptPrintList("BSV_MINIMAL_FILES" "${BSV_MINIMAL_FILES}")## Help to debug

## Helper files activate some settings for bsv and secp256k1 native source codes
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-bitcoin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-secp256k1.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-univalue.cmake")

## Log list of bsv source files
message(STATUS "Build core using bsv source code in ${SCRYPT_BSV_SOURCE_ROOT}")
foreach(_sv_file ${BSV_MINIMAL_FILES})
  message(STATUS "    [${_sv_file}]")
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_sv_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${SCRYPT_BSV_SOURCE_ROOT} PREFIX "Sources" FILES "${_sv_file}")
  else()
    source_group(TREE ${SCRYPT_BSV_SOURCE_ROOT} PREFIX "Headers" FILES "${_sv_file}")
  endif()
endforeach()
source_group("_generated" FILES "${BITCOIN_CONFIG_FILE}")

## Build scrypt_core as a library using bsv source code
add_library(scrypt_core ${BITCOIN_CONFIG_FILE} ${BSV_MINIMAL_HDR_FILES} ${BSV_MINIMAL_SRC_FILES})
target_include_directories(scrypt_core PUBLIC ${BSV_INCLUDE_DIRS})
target_link_libraries(scrypt_core univalue secp256k1 OpenSSL::Crypto OpenSSL::SSL ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
if(WIN32)
  target_link_libraries(scrypt_core Crypt32.lib Ws2_32)
endif()
target_compile_definitions(scrypt_core PUBLIC HAVE_CONFIG_H)
scryptLinkTargetToBoost(scrypt_core Boost::boost)

set_property(TARGET scrypt_core PROPERTY FOLDER "core")

## Install scrypt_core : the *.h header files should be kept as bsv structure
install(FILES ${BITCOIN_CONFIG_FILE} DESTINATION "include/sv/config" COMPONENT scrypt_core)
foreach(_bsv_hdr ${BSV_MINIMAL_HDR_FILES})
  get_filename_component(_dir ${_bsv_hdr} DIRECTORY)
  get_filename_component(_dir ${_bsv_hdr} DIRECTORY)
  string(REPLACE "${SCRYPT_BSV_SOURCE_ROOT}/" "" _relative_dir "${_dir}")
  set(_install_dest "include/sv/${_relative_dir}")
  #message("install file [${_bsv_hdr}]  _install_dest[${_install_dest}]")
  install(FILES ${_bsv_hdr} DESTINATION "${_install_dest}" COMPONENT scrypt_core)
endforeach()
install(TARGETS scrypt_core DESTINATION "lib" COMPONENT scrypt_core)