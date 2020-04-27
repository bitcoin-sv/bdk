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
## BUILD sv
## ./autogen.sh
## export PKG_CONFIG_PATH=/path/to/openssl/lib/pkgconfig:$PKG_CONFIG_PATH
## ./configure --with-boost=/path/to/boost_1_63_0 --disable-ccache --disable-maintainer-mode --disable-dependency-tracking --enable-glibc-back-compat --enable-reduce-exports --disable-bench ; make -j4


function(_checkprint_SCRYPT_BSV_ROOT_DIR)
  if(DEFINED SCRYPT_BSV_ROOT_DIR)
    if(EXISTS "${SCRYPT_BSV_ROOT_DIR}")
        message(STATUS "Found Bitcoin SV source code at SCRYPT_BSV_ROOT_DIR=[${SCRYPT_BSV_ROOT_DIR}]")
    else()
        message(FATAL_ERROR "Used inexisting directory SCRYPT_BSV_ROOT_DIR=[${SCRYPT_BSV_ROOT_DIR}]")
    endif()
  else()
    message(FATAL_ERROR "Variable SCRYPT_BSV_ROOT_DIR is not defined")
  endif()
endfunction()

function(cryptFindBSVDir)########################################################################################
  ## Function try to find the BSV source code and set the cache variable SCRYPT_BSV_ROOT_DIR
  ##   If user define -DBSV_ROOT, it will be use
  ##   If user define environment variable BSV_ROOT, it will be used
  ##   If inside the bscrypt project, there are a directory '${CMAKE_SOURCE_DIR}/sv', then it will be used      ## jenkins build
  ##   If alongside the bscrypt project, there are a directory ${CMAKE_SOURCE_DIR}/../sv, then it will be used  ## local development
  ##   If non of them exist, then it will be clone from source into '${CMAKE_SOURCE_DIR}/sv'
  if(DEFINED BSV_ROOT)
    file(TO_CMAKE_PATH "${BSV_ROOT}" _CMAKE_PATH)
    set(SCRYPT_BSV_ROOT_DIR "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  if(DEFINED ENV{BSV_ROOT})
    file(TO_CMAKE_PATH "$ENV{BSV_ROOT}" _CMAKE_PATH)
    set(SCRYPT_BSV_ROOT_DIR "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  if(EXISTS "${CMAKE_SOURCE_DIR}/sv")
    set(SCRYPT_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/sv" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  if(EXISTS "${CMAKE_SOURCE_DIR}/../sv")
    get_filename_component(_ABS_PATH "${CMAKE_SOURCE_DIR}/../sv" ABSOLUTE)
    set(SCRYPT_BSV_ROOT_DIR "${_ABS_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  set(BSV_REPO_URL "git@bitbucket.org:nch-atlassian/sv.git")#Private dev repo
  message(STATUS "Unable to find bsv source code on local machine. It will be clone to [${CMAKE_SOURCE_DIR}/sv]")
  execute_process(
    COMMAND git clone ${BSV_REPO_URL}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/sv")
    message(FATAL_ERROR "Unable to clone [${BSV_REPO_URL}] into [${CMAKE_SOURCE_DIR}/sv]. Check internet connection")
  else()
    message(STATUS "Successfully cloned [${BSV_REPO_URL}] into [${CMAKE_SOURCE_DIR}/sv]")
    set(SCRYPT_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/sv" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
  endif()
endfunction()##cryptFindBSVDir

function(cryptGetMinimumListBSVSource)########################################################################################
  if(NOT (DEFINED SCRYPT_BSV_ROOT_DIR AND EXISTS "${SCRYPT_BSV_ROOT_DIR}"))
      message(FATAL_ERROR "Unable to locate bsv source code for SCRYPT_BSV_ROOT_DIR")
  endif()

  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_ROOT_DIR}/src")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_ROOT_DIR}/src/crypto")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_ROOT_DIR}/src/script")

  set(_minimal_hdr_files
      "src/amount.h"  ##  Used by [interpreter.cpp]
      "src/big_int.h"  ##  Used by [], [interpreter.cpp], [script.cpp], [script_num.cpp]
      "src/compat/byteswap.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha512.cpp]
      "src/compat/endian.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha512.cpp]
      "src/consensus/consensus.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp], [script.cpp]
      "src/crypto/common.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha512.cpp]
      "src/crypto/hmac_sha512.h"  ##  Used by [hash.cpp], [hmac_sha512.cpp]
      "src/crypto/ripemd160.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [ripemd160.cpp]
      "src/crypto/sha1.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp], [sha1.cpp]
      "src/crypto/sha256.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp]
      "src/crypto/sha512.h"  ##  Used by [hash.cpp], [hmac_sha512.cpp], [sha512.cpp]
      "src/hash.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp]
      "src/httpserver.h"  ##  Used by [script.cpp]
      "src/prevector.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [script.cpp]
      "src/primitives/transaction.h"  ##  Used by [interpreter.cpp]
      "src/pubkey.h"  ##  Used by [hash.cpp], [interpreter.cpp], [pubkey.cpp]
      "src/rpc/text_writer.h"  ##  Used by [script.cpp]
      "src/script/int_serialization.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp]
      "src/script/limitedstack.h"  ##  Used by [limitedstack.cpp]
      "src/script/script.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp]
      "src/script/script_flags.h"  ##  Used by [interpreter.cpp]
      "src/script/script_num.h"  ##  Used by [interpreter.cpp]
      "src/script/interpreter.h"  ##  Used by [interpreter.cpp]
      "src/script/script_error.h"  ##  Used by [interpreter.cpp]
      "src/script/sighashtype.h"  ##  Used by [interpreter.cpp]
      "src/script_config.h"  ##  Used by [interpreter.cpp]
      # "src/secp256k1/include/secp256k1.h"  ##  Used by [pubkey.cpp]
      # "src/secp256k1/include/secp256k1_recovery.h"  ##  Used by [pubkey.cpp]
      "src/serialize.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp], [script.cpp]
      "src/taskcancellation.h"  ##  Used by [interpreter.cpp]
      "src/tinyformat.h"  ##  Used by [script.cpp]
      "src/uint256.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp]
      "src/utilstrencodings.h"  ##  Used by [script.cpp]
      "src/version.h"  ##  Used by [hash.cpp], [interpreter.cpp], [limitedstack.cpp], [pubkey.cpp]
      ## Total number of header files : 34
  )
  foreach(rel_file ${_minimal_hdr_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_HDR_FILES "${SCRYPT_BSV_ROOT_DIR}/${rel_file}")
  endforeach()

  set(_minimal_src_files
      "src/big_int.cpp"
      "src/crypto/hmac_sha512.cpp"
      "src/crypto/ripemd160.cpp"
      "src/crypto/sha1.cpp"
      "src/crypto/sha512.cpp"
      "src/hash.cpp"
      "src/pubkey.cpp"
      "src/script/interpreter.cpp"
      "src/script/limitedstack.cpp"
      "src/script/script.cpp"
      "src/script/script_num.cpp"
      ## Total number of source files : 11
  )
  foreach(rel_file ${_minimal_src_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_SRC_FILES "${SCRYPT_BSV_ROOT_DIR}/${rel_file}")
  endforeach()
endfunction()



macro(HelpFindBSVSource)########################################################################################
  cryptFindBSVDir()
  cryptGetMinimumListBSVSource()
endmacro()

function(scryptPrintBSVSourceInfo)
  message("BSV Source info :")
  message("SCRYPT_BSV_ROOT_DIR  [${SCRYPT_BSV_ROOT_DIR}]")

  scryptPrintList("BSV_MINIMAL_HDR_FILES" "${BSV_MINIMAL_HDR_FILES}")
  scryptPrintList("BSV_MINIMAL_SRC_FILES" "${BSV_MINIMAL_SRC_FILES}")
  scryptPrintList("BSV_INCLUDE_DIRS" "${BSV_INCLUDE_DIRS}")
endfunction()
