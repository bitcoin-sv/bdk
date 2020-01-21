#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindBSVSourceHelper_Include)
  return()
endif()
set(FindBSVSourceHelper_Include TRUE)
include(${CMAKE_SOURCE_DIR}/cmake/SCRYPTTools.cmake)


## Locate the BSV source root
## Read some useful informations from bsv sources and store cache variables
## TODO : if fail, it might clone/checkout from internet
##
macro(cryptFindBSVDir)########################################################################################
  ## Try to determine BSV source root. The priorities are in the order below:
  ##   If user define -DSCRYPT_BSV_SRC_ROOT, it will be use
  ##   If user define environment variable SCRYPT_BSV_SRC_ROOT, it will be used
  ##   If alongside the scrypt project, there are a directory 'sv', then it will be used
  if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)
    if(DEFINED ENV{SCRYPT_BSV_SRC_ROOT})
      file(TO_CMAKE_PATH "$ENV{SCRYPT_BSV_SRC_ROOT}" _CMAKE_PATH)
      set(SCRYPT_BSV_SRC_ROOT "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
      message(STATUS "Environment variable SCRYPT_BSV_SRC_ROOT[${SCRYPT_BSV_SRC_ROOT}] is used to look for bsv source files")
    endif()
  endif()

  if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)# Failed to look up for environment variable
    get_filename_component(_BSV_ABS_PATH "${CMAKE_SOURCE_DIR}/../sv" ABSOLUTE)
    if(EXISTS "${_BSV_ABS_PATH}")
      set(SCRYPT_BSV_SRC_ROOT "${_BSV_ABS_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
      message(STATUS "[${SCRYPT_BSV_SRC_ROOT}] is used to look for bsv source files")
    endif()
  endif()

  if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)# If till the end cannot find the bsv source code, then fails the build
      message(FATAL_ERROR "Unable to locate bsv source code for SCRYPT_BSV_SRC_ROOT")
  endif()
endmacro()

macro(cryptGetMinimumListBSVSource)########################################################################################
  if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)#
      message(FATAL_ERROR "Unable to locate bsv source code for SCRYPT_BSV_SRC_ROOT")
  endif()

  set(_minimal_files
      src/big_int.cpp
      src/crypto/hmac_sha512.cpp
      src/crypto/ripemd160.cpp
      src/crypto/sha1.cpp
      src/crypto/sha512.cpp
      src/hash.cpp
      src/pubkey.cpp
      src/script/interpreter.cpp
      src/script/limitedstack.cpp
      src/script/script.cpp
      src/script/script_num.cpp
  )

  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SRC_ROOT}/src")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SRC_ROOT}/src/crypto")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SRC_ROOT}/src/script")

  foreach(rel_file ${_minimal_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_SRC_FILES "${SCRYPT_BSV_SRC_ROOT}/${rel_file}")
  endforeach()
endmacro()



macro(HelpFindBSVSource)########################################################################################
  cryptFindBSVDir()
  cryptGetMinimumListBSVSource()
endmacro()

function(scryptPrintBSVSourceInfo)
  message("BSV Source info :")
  message("SCRYPT_BSV_SRC_ROOT  [${SCRYPT_BSV_SRC_ROOT}]")
  scryptPrintList("BSV_MINIMAL_SRC_FILES" "${BSV_MINIMAL_SRC_FILES}")
  scryptPrintList("BSV_INCLUDE_DIRS" "${BSV_INCLUDE_DIRS}")
endfunction()
