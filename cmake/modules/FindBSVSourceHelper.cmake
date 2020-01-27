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
      "src/amount.h"  ##  Used by [block.cpp], [interpreter.cpp], [merkle.cpp], [standard.cpp], [transaction.cpp]
      "src/big_int.h"  ##  Used by [block.cpp], [interpreter.cpp], [script.cpp], [script_num.cpp], [standard.cpp]
      "src/bloom.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/compat/byteswap.h"  ##  Used by [chacha20.cpp], [limitedstack.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp]
      "src/compat/endian.h"  ##  Used by [chacha20.cpp], [limitedstack.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp]
      "src/consensus/consensus.h"  ##  Used by [block.cpp], [interpreter.cpp], [jsonwriter.cpp], [limitedstack.cpp], [merkle.cpp], [script.cpp], [standard.cpp], [transaction.cpp]
      "src/consensus/validation.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/crypto/chacha20.h"  ##  Used by [chacha20.cpp]
      "src/crypto/common.h"  ##  Used by [block.cpp], [chacha20.cpp], [interpreter.cpp], [limitedstack.cpp], [merkle.cpp], [ripemd160.cpp], [script.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp], [standard.cpp], [transaction.cpp]
      "src/crypto/hmac_sha256.h"  ##  Used by [hmac_sha256.cpp]
      "src/crypto/hmac_sha512.h"  ##  Used by [hmac_sha512.cpp]
      "src/crypto/ripemd160.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp], [ripemd160.cpp]
      "src/crypto/sha1.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp], [sha1.cpp]
      "src/crypto/sha256.h"  ##  Used by [hmac_sha256.cpp], [interpreter.cpp], [limitedstack.cpp], [sha256.cpp]
      "src/crypto/sha512.h"  ##  Used by [hmac_sha512.cpp], [sha512.cpp]
      "src/enum_cast.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/hash.h"  ##  Used by [block.cpp], [limitedstack.cpp], [merkle.cpp], [transaction.cpp]
      "src/httpserver.h"  ##  Used by [jsonwriter.cpp]
      "src/mining/candidates.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/journal.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/journal_entry.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/legacy.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/prevector.h"  ##  Used by [limitedstack.cpp], [script.cpp]
      "src/primitives/block.h"  ##  Used by [block.cpp], [merkle.cpp]
      "src/primitives/transaction.h"  ##  Used by [block.cpp], [interpreter.cpp], [merkle.cpp], [standard.cpp], [transaction.cpp]
      "src/pubkey.h"  ##  Used by [interpreter.cpp], [standard.cpp]
      "src/script/int_serialization.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp]
      "src/script/interpreter.h"  ##  Used by [standard.cpp]
      "src/script/limitedstack.h"  ##  Used by [limitedstack.cpp]
      "src/script/script.h"  ##  Used by [block.cpp], [interpreter.cpp], [limitedstack.cpp], [merkle.cpp], [standard.cpp], [transaction.cpp]
      "src/script/script_flags.h"  ##  Used by [interpreter.cpp], [standard.cpp]
      "src/script/script_num.h"  ##  Used by [block.cpp], [interpreter.cpp], [standard.cpp]
      "src/script/standard.h"  ##  Used by [standard.cpp]
      "src/script_config.h"  ##  Used by [interpreter.cpp]
      "src/serialize.h"  ##  Used by [limitedstack.cpp], [script.cpp]
      "src/support/cleanse.h"  ##  Used by [lockedpool.cpp]
      "src/support/lockedpool.h"  ##  Used by [lockedpool.cpp]
      "src/taskcancellation.h"  ##  Used by [interpreter.cpp]
      "src/tinyformat.h"  ##  Used by [block.cpp], [script.cpp], [transaction.cpp]
      "src/tx_mempool_info.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/uint256.h"  ##  Used by [block.cpp], [interpreter.cpp], [merkle.cpp], [standard.cpp], [transaction.cpp]
      "src/util.h"  ##  Used by [standard.cpp]
      "src/utilstrencodings.h"  ##  Used by [block.cpp], [merkle.cpp], [script.cpp], [standard.cpp], [transaction.cpp]
      "src/utiltime.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/addrdb.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/addrman.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/arith_uint256.h"  ##  Used by [config.cpp], [dstencode.cpp], [key.cpp]
      "src/base58.h"  ##  Used by [base58.cpp], [dstencode.cpp]
      "src/blockstreams.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/blockvalidation.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/chain.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/chainparams.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp]
      "src/chainparamsbase.h"  ##  Used by [base58.cpp], [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [dstencode.cpp], [util.cpp]
      "src/chainparamsseeds.h"  ##  Used by [chainparams.cpp]
      "src/coins.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/compat.h"  ##  Used by [base58.cpp], [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [logging.cpp], [random.cpp], [standard.cpp], [util.cpp]
      "src/compressor.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/config.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/consensus/merkle.h"  ##  Used by [chainparams.cpp]
      "src/consensus/params.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp]
      "src/core_io.h"  ##  Used by [core_read.cpp], [core_write.cpp]
      "src/core_memusage.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/dstencode.h"  ##  Used by [core_write.cpp], [dstencode.cpp]
      "src/fs.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fs.cpp], [logging.cpp], [random.cpp], [standard.cpp], [util.cpp]
      "src/indirectmap.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/key.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [key.cpp]
      "src/limitedmap.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/logging.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [logging.cpp], [random.cpp], [standard.cpp], [util.cpp]
      "src/memusage.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/assembler.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/factory.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/mining/journal_change_set.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/net.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/netaddress.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp]
      "src/policy/policy.h"  ##  Used by [chainparams.cpp], [config.cpp], [dstencode.cpp]
      "src/pow.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/protocol.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp]
      "src/random.h"  ##  Used by [config.cpp], [dstencode.cpp], [key.cpp], [random.cpp], [util.cpp]
      "src/rpc/jsonwriter.h"  ##  Used by [core_read.cpp], [core_write.cpp]
      "src/rpc/protocol.h"  ##  Used by [core_write.cpp]
      "src/rpc/server.h"  ##  Used by [core_write.cpp]
      "src/rpc/text_writer.h"  ##  Used by [block.cpp], [chainparams.cpp], [merkle.cpp], [random.cpp], [script.cpp], [standard.cpp], [transaction.cpp], [uint256.cpp], [util.cpp], [utilmoneystr.cpp], [utilstrencodings.cpp]
      "src/script/script_error.h"  ##  Used by [config.cpp]
      "src/script/sighashtype.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [interpreter.cpp], [standard.cpp]
      "src/streams.h"  ##  Used by [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp]
      "src/support/allocators/secure.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [key.cpp]
      "src/support/allocators/zeroafterfree.h"  ##  Used by [base58.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp]
      "src/sync.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [logging.cpp], [random.cpp], [standard.cpp], [util.cpp]
      "src/task.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/task_helpers.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/threadinterrupt.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/threadpool.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/threadpoolT.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/threadpriority.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/threadsafety.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [logging.cpp], [random.cpp], [standard.cpp], [util.cpp]
      "src/time_locked_mempool.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/timedata.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txmempool.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txn_double_spend_detector.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txn_sending_details.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txn_validation_config.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txn_validation_data.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/txn_validation_result.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/utilmoneystr.h"  ##  Used by [core_write.cpp], [utilmoneystr.cpp]
      "src/validation.h"  ##  Used by [config.cpp], [dstencode.cpp]
      "src/version.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [standard.cpp], [transaction.cpp]
      "src/versionbits.h"  ##  Used by [config.cpp], [dstencode.cpp]
      ## Total number of header files : 107
  )
  foreach(rel_file ${_minimal_hdr_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_HDR_FILES "${SCRYPT_BSV_SOURCE_ROOT}/${rel_file}")
  endforeach()

  set(_minimal_src_files
      "src/base58.cpp"
      "src/big_int.cpp"
      "src/chainparams.cpp"
      "src/chainparamsbase.cpp"
      "src/config.cpp"
      "src/consensus/merkle.cpp"
      "src/core_read.cpp"
      "src/core_write.cpp"
      "src/crypto/chacha20.cpp"
      "src/crypto/hmac_sha256.cpp"
      "src/crypto/hmac_sha512.cpp"
      "src/crypto/ripemd160.cpp"
      "src/crypto/sha1.cpp"
      "src/crypto/sha256.cpp"
      "src/crypto/sha256_sse4.cpp"
      "src/crypto/sha512.cpp"
      "src/dstencode.cpp"
      "src/fs.cpp"
      "src/hash.cpp"
      "src/key.cpp"
      "src/logging.cpp"
      "src/primitives/block.cpp"
      "src/primitives/transaction.cpp"
      "src/pubkey.cpp"
      "src/random.cpp"
      "src/rpc/jsonwriter.cpp"
      "src/script/interpreter.cpp"
      "src/script/limitedstack.cpp"
      "src/script/script.cpp"
      "src/script/script_num.cpp"
      "src/script/standard.cpp"
      "src/support/cleanse.cpp"
      "src/support/lockedpool.cpp"
      "src/uint256.cpp"
      "src/util.cpp"
      "src/utilmoneystr.cpp"
      "src/utilstrencodings.cpp"
      "src/utiltime.cpp"
      ## Total number of source files : 38
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
