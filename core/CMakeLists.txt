#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if (NOT (Boost_FOUND AND OPENSSL_FOUND))
  message(FATAL_ERROR "Boost and OpenSSL are required to build scrypt-core")
endif()

## Build scrypt requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)#
  message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_SRC_ROOT")
endif()
#scryptPrintList("BSV_MINIMAL_SRC_FILES" "${BSV_MINIMAL_SRC_FILES}")## Help to debug

## Helper files activate some settings for bsv and secp256k1 native source codes
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-secp256k1.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/setting-bitcoin.cmake")

## Log list of bsv source files
message(STATUS "Build using bsv source code in ${SCRYPT_BSV_SRC_ROOT}")
get_filename_component(_BSV_PARENT_DIR "${SCRYPT_BSV_SRC_ROOT}" DIRECTORY )
foreach(_sv_file ${BSV_MINIMAL_SRC_FILES})
  message(STATUS "    [${_sv_file}]")
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_sv_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${_BSV_PARENT_DIR} PREFIX "BSV Source" FILES "${_sv_file}")
  else()
    source_group(TREE ${_BSV_PARENT_DIR} PREFIX "BSV Header" FILES "${_sv_file}")
  endif()
endforeach()

## Build core-scrypt as a library using bsv source code
add_library(core-scrypt ${BSV_MINIMAL_SRC_FILES})
target_include_directories(core-scrypt PUBLIC ${BSV_INCLUDE_DIRS})
link_secp256k1_internal(core-scrypt)
target_link_libraries(core-scrypt OpenSSL::Crypto OpenSSL::SSL ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
scryptLinkTargetToBoost(core-scrypt Boost::boost)



set_property(TARGET core-scrypt PROPERTY FOLDER "core")