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
  ##   If user define -DSCRYPT_BSV_SOURCE_ROOT, it will be use
  ##   If user define environment variable SCRYPT_BSV_SOURCE_ROOT, it will be used
  ##   If alongside the scrypt project, there are a directory 'sv', then it will be used
  if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)
    if(DEFINED ENV{SCRYPT_BSV_SOURCE_ROOT})
      file(TO_CMAKE_PATH "$ENV{SCRYPT_BSV_SOURCE_ROOT}" _CMAKE_PATH)
      set(SCRYPT_BSV_SOURCE_ROOT "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
      message(STATUS "Environment variable SCRYPT_BSV_SOURCE_ROOT[${SCRYPT_BSV_SOURCE_ROOT}] is used to look for bsv source files")
    endif()
  endif()

  if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)# Failed to look up for environment variable
    get_filename_component(_BSV_ABS_PATH "${CMAKE_SOURCE_DIR}/../sv" ABSOLUTE)
    if(EXISTS "${_BSV_ABS_PATH}")
      set(SCRYPT_BSV_SOURCE_ROOT "${_BSV_ABS_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
      message(STATUS "[${SCRYPT_BSV_SOURCE_ROOT}] is used to look for bsv source files")
    endif()
  endif()

  if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)# If till the end cannot find the bsv source code, then fails the build
      message(FATAL_ERROR "Unable to locate bsv source code for SCRYPT_BSV_SOURCE_ROOT")
  endif()
endmacro()

macro(cryptGetMinimumListBSVSource)########################################################################################
  if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)#
      message(FATAL_ERROR "Unable to locate bsv source code for SCRYPT_BSV_SOURCE_ROOT")
  endif()

  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SOURCE_ROOT}/src")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SOURCE_ROOT}/src/crypto")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SCRYPT_BSV_SOURCE_ROOT}/src/script")

  set(_minimal_hdr_files
      src/addrdb.h  ##  Used by [interpreter.cpp]
      src/addrman.h  ##  Used by [interpreter.cpp]
      src/arith_uint256.h  ##  Used by [interpreter.cpp]
      src/big_int.h  ##  Used by [big_int.cpp]
      src/blockstreams.h  ##  Used by [interpreter.cpp]
      src/blockvalidation.h  ##  Used by [interpreter.cpp]
      src/chain.h  ##  Used by [interpreter.cpp]
      src/chainparams.h  ##  Used by [interpreter.cpp]
      src/chainparamsbase.h  ##  Used by [interpreter.cpp]
      src/coins.h  ##  Used by [interpreter.cpp]
      src/compat.h  ##  Used by [interpreter.cpp]
      src/compat/byteswap.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp]
      src/compat/endian.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp]
      src/compressor.h  ##  Used by [interpreter.cpp]
      src/consensus/params.h  ##  Used by [interpreter.cpp]
      src/core_memusage.h  ##  Used by [interpreter.cpp]
      src/crypto/chacha20.h  ##  Used by [interpreter.cpp]
      src/crypto/common.h  ##  Used by [hash.cpp],[ pubkey.cpp]
      src/crypto/hmac_sha512.h  ##  Used by [hash.cpp]
      src/crypto/ripemd160.h  ##  Used by [hash.cpp],[ pubkey.cpp]
      src/crypto/sha256.h  ##  Used by [hash.cpp],[ pubkey.cpp]
      src/crypto/sha512.h  ##  Used by [hash.cpp]
      src/fs.h  ##  Used by [interpreter.cpp]
      src/hash.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp]
      src/indirectmap.h  ##  Used by [interpreter.cpp]
      src/limitedmap.h  ##  Used by [interpreter.cpp]
      src/logging.h  ##  Used by [interpreter.cpp]
      src/memusage.h  ##  Used by [interpreter.cpp]
      src/mining/assembler.h  ##  Used by [interpreter.cpp]
      src/mining/factory.h  ##  Used by [interpreter.cpp]
      src/mining/journal_change_set.h  ##  Used by [interpreter.cpp]
      src/net.h  ##  Used by [interpreter.cpp]
      src/netaddress.h  ##  Used by [interpreter.cpp]
      src/policy/policy.h  ##  Used by [interpreter.cpp]
      src/pow.h  ##  Used by [interpreter.cpp]
      src/prevector.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp]
      src/primitives/block.h  ##  Used by [interpreter.cpp]
      src/protocol.h  ##  Used by [interpreter.cpp],[ interpreter.cpp],[ interpreter.cpp],[ interpreter.cpp],[ interpreter.cpp]
      src/pubkey.h  ##  Used by [hash.cpp],[ pubkey.cpp]
      src/random.h  ##  Used by [interpreter.cpp]
      src/script/standard.h  ##  Used by [interpreter.cpp]
      src/script/int_serialization.h  ##  Used by [script.cpp],[ script_num.cpp]
      src/script/interpreter.h  ##  Used by [interpreter.cpp]
      src/script/limitedstack.h  ##  Used by [interpreter.cpp]
      src/script/script.h  ##  Used by [script.cpp]
      src/script/script_error.h  ##  Used by [interpreter.cpp]
      src/script/script_num.h  ##  Used by [script.cpp],[ script_num.cpp]
      src/script/sighashtype.h  ##  Used by [interpreter.cpp]
      src/serialize.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp]
      src/streams.h  ##  Used by [interpreter.cpp]
      src/support/allocators/zeroafterfree.h  ##  Used by [interpreter.cpp]
      src/support/cleanse.h  ##  Used by [interpreter.cpp]
      src/sync.h  ##  Used by [interpreter.cpp]
      src/task.h  ##  Used by [interpreter.cpp]
      src/task_helpers.h  ##  Used by [interpreter.cpp]
      src/threadinterrupt.h  ##  Used by [interpreter.cpp]
      src/threadpool.h  ##  Used by [interpreter.cpp]
      src/threadpoolT.h  ##  Used by [interpreter.cpp]
      src/threadpriority.h  ##  Used by [interpreter.cpp]
      src/threadsafety.h  ##  Used by [interpreter.cpp]
      src/time_locked_mempool.h  ##  Used by [interpreter.cpp]
      src/timedata.h  ##  Used by [interpreter.cpp]
      src/tinyformat.h  ##  Used by [interpreter.cpp]
      src/txmempool.h  ##  Used by [interpreter.cpp]
      src/txn_double_spend_detector.h  ##  Used by [interpreter.cpp]
      src/txn_sending_details.h  ##  Used by [interpreter.cpp]
      src/txn_validation_config.h  ##  Used by [interpreter.cpp]
      src/txn_validation_data.h  ##  Used by [interpreter.cpp]
      src/txn_validation_result.h  ##  Used by [interpreter.cpp]
      src/uint256.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ limitedstack.cpp]
      src/util.h  ##  Used by [interpreter.cpp]
      src/validation.h  ##  Used by [interpreter.cpp]
      src/version.h  ##  Used by [hash.cpp],[ pubkey.cpp],[ interpreter.cpp],[ limitedstack.cpp]
      src/versionbits.h  ##  Used by [interpreter.cpp]
  )
  foreach(rel_file ${_minimal_hdr_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_HDR_FILES "${SCRYPT_BSV_SOURCE_ROOT}/${rel_file}")
  endforeach()

  set(_minimal_src_files
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
  foreach(rel_file ${_minimal_src_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_SRC_FILES "${SCRYPT_BSV_SOURCE_ROOT}/${rel_file}")
  endforeach()
endmacro()



macro(HelpFindBSVSource)########################################################################################
  cryptFindBSVDir()
  cryptGetMinimumListBSVSource()
endmacro()

function(scryptPrintBSVSourceInfo)
  message("BSV Source info :")
  message("SCRYPT_BSV_SOURCE_ROOT  [${SCRYPT_BSV_SOURCE_ROOT}]")
  scryptPrintList("BSV_MINIMAL_HDR_FILES" "${BSV_MINIMAL_HDR_FILES}")
  scryptPrintList("BSV_MINIMAL_SRC_FILES" "${BSV_MINIMAL_SRC_FILES}")
  scryptPrintList("BSV_INCLUDE_DIRS" "${BSV_INCLUDE_DIRS}")
endfunction()
