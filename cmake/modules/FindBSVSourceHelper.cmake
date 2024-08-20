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
include(${CMAKE_SOURCE_DIR}/cmake/SESDKTools.cmake)


## Locate the BSV source root
## Read some useful informations from bsv sources and store cache variables
## BUILD sv
## ./autogen.sh
## export PKG_CONFIG_PATH=/path/to/openssl/lib/pkgconfig:$PKG_CONFIG_PATH
## ./configure --with-boost=/path/to/boost_1_63_0 --disable-ccache --disable-maintainer-mode --disable-dependency-tracking --enable-glibc-back-compat --enable-reduce-exports --disable-bench ; make -j4


function(_checkprint_SESDK_BSV_ROOT_DIR)
  if(DEFINED SESDK_BSV_ROOT_DIR)
    if(EXISTS "${SESDK_BSV_ROOT_DIR}")
        message(STATUS "Found Bitcoin SV source code at SESDK_BSV_ROOT_DIR=[${SESDK_BSV_ROOT_DIR}]")
    else()
        message(FATAL_ERROR "Used inexisting directory SESDK_BSV_ROOT_DIR=[${SESDK_BSV_ROOT_DIR}]")
    endif()
  else()
    message(FATAL_ERROR "Variable SESDK_BSV_ROOT_DIR is not defined")
  endif()
endfunction()

function(cryptFindBSVDir)########################################################################################
  ## Function try to find the BSV source code and set the cache variable SESDK_BSV_ROOT_DIR
  ##   If user define -DBSV_ROOT, it will be use
  ##   If user define environment variable BSV_ROOT, it will be used
  ##   If inside the script engine project, there are a directory '${CMAKE_SOURCE_DIR}/sv', then it will be used      ## jenkins build
  ##   If alongside the script engine project, there are a directory ${CMAKE_SOURCE_DIR}/../sv, then it will be used  ## local development
  ##   If non of them exist, then it will be clone from source into '${CMAKE_SOURCE_DIR}/sv'
  if(DEFINED BSV_ROOT)
    file(TO_CMAKE_PATH "${BSV_ROOT}" _CMAKE_PATH)
    set(SESDK_BSV_ROOT_DIR "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SESDK_BSV_ROOT_DIR()
    return()
  endif()

  if(DEFINED ENV{BSV_ROOT})
    file(TO_CMAKE_PATH "$ENV{BSV_ROOT}" _CMAKE_PATH)
    set(SESDK_BSV_ROOT_DIR "${_CMAKE_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SESDK_BSV_ROOT_DIR()
    return()
  endif()

  if(EXISTS "${CMAKE_SOURCE_DIR}/sv")
    set(SESDK_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/sv" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SESDK_BSV_ROOT_DIR()
    return()
  endif()

  if(EXISTS "${CMAKE_SOURCE_DIR}/../sv")
    get_filename_component(_ABS_PATH "${CMAKE_SOURCE_DIR}/../sv" ABSOLUTE)
    set(SESDK_BSV_ROOT_DIR "${_ABS_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SESDK_BSV_ROOT_DIR()
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
    set(SESDK_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/sv" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SESDK_BSV_ROOT_DIR()
  endif()
endfunction()##cryptFindBSVDir

function(cryptGetMinimumListBSVSource)########################################################################################
  if(NOT (DEFINED SESDK_BSV_ROOT_DIR AND EXISTS "${SESDK_BSV_ROOT_DIR}"))
      message(FATAL_ERROR "Unable to locate bsv source code for SESDK_BSV_ROOT_DIR")
  endif()

  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SESDK_BSV_ROOT_DIR}/src")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SESDK_BSV_ROOT_DIR}/src/crypto")
  scryptAppendToGlobalSet(BSV_INCLUDE_DIRS "${SESDK_BSV_ROOT_DIR}/src/script")

  set(_minimal_hdr_files
      "src/addrdb.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/addrman.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/amount.h"  ##  Used by [amount.cpp], [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [merkle.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [utilmoneystr.cpp]
      "src/arith_uint256.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [key.cpp], [scriptcache.cpp]
      "src/base58.h"  ##  Used by [base58.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/big_int.h"  ##  Used by [big_int.cpp], [block.cpp], [chainparams.cpp], [core_write.cpp], [interpreter.cpp], [script.cpp], [script_num.cpp], [standard.cpp]
      "src/blockstreams.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/blockvalidation.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/bloom.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/chain.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/chainparams.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/chainparamsbase.h"  ##  Used by [base58.cpp], [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [dstencode.cpp], [interpreter.cpp], [util.cpp]
      "src/chainparamsseeds.h"  ##  Used by [chainparams.cpp]
      "src/coins.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/compat.h"  ##  Used by [base58.cpp], [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/compat/byteswap.h"  ##  Used by [amount.cpp], [assembler.cpp], [base58.cpp], [block.cpp], [chacha20.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [random.cpp], [ripemd160.cpp], [script.cpp], [scriptcache.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/compat/endian.h"  ##  Used by [amount.cpp], [assembler.cpp], [base58.cpp], [block.cpp], [chacha20.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [random.cpp], [ripemd160.cpp], [script.cpp], [scriptcache.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/compressor.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/config.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/consensus/consensus.h"  ##  Used by [assembler.cpp], [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [jsonwriter.cpp], [merkle.cpp], [random.cpp], [script.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp], [utilstrencodings.cpp]
      "src/consensus/merkle.h"  ##  Used by [chainparams.cpp]
      "src/consensus/params.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/consensus/validation.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/core_io.h"  ##  Used by [assembler.cpp], [core_read.cpp], [core_write.cpp], [interpreter.cpp]
      "src/core_memusage.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/crypto/chacha20.h"  ##  Used by [chacha20.cpp], [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [key.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [util.cpp]
      "src/crypto/common.h"  ##  Used by [assembler.cpp], [base58.cpp], [block.cpp], [chacha20.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [random.cpp], [ripemd160.cpp], [script.cpp], [scriptcache.cpp], [sha1.cpp], [sha256.cpp], [sha512.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/crypto/hmac_sha256.h"  ##  Used by [hmac_sha256.cpp]
      "src/crypto/hmac_sha512.h"  ##  Used by [hash.cpp], [hmac_sha512.cpp], [key.cpp]
      "src/crypto/ripemd160.h"  ##  Used by [base58.cpp], [block.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [ripemd160.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp]
      "src/crypto/sha1.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp], [sha1.cpp]
      "src/crypto/sha256.h"  ##  Used by [base58.cpp], [block.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [hmac_sha256.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [scriptcache.cpp], [sha256.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp]
      "src/crypto/sha512.h"  ##  Used by [hash.cpp], [hmac_sha512.cpp], [key.cpp], [random.cpp], [sha512.cpp]
      "src/cuckoocache.h"  ##  Used by [scriptcache.cpp], [sigcache.cpp]
      "src/dstencode.h"  ##  Used by [core_write.cpp], [dstencode.cpp]
      "src/enum_cast.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/fs.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [fs.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/hash.h"  ##  Used by [base58.cpp], [block.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp]
      "src/indirectmap.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [sigcache.cpp]
      "src/key.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [interpreter.cpp], [key.cpp]
      "src/keystore.h"  ##  Manual fix the build
      "src/limitedmap.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/logging.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/memusage.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [sigcache.cpp]
      "src/net/association.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/net.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/net_message.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/net_types.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/netaddress.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/net/node_stats.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/send_queue_bytes.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/net/stream.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/orphan_txns.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/policy/policy.h"  ##  Used by [chainparams.cpp], [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/pow.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/prevector.h"  ##  Used by [amount.cpp], [assembler.cpp], [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [script.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/primitives/block.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [merkle.cpp], [scriptcache.cpp]
      "src/primitives/transaction.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [merkle.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [utilmoneystr.cpp]
      "src/protocol.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/pubkey.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [pubkey.cpp], [sigcache.cpp], [standard.cpp]
      "src/random.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [key.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [util.cpp]
      "src/rpc/jsonwriter.h"  ##  Used by [assembler.cpp], [core_read.cpp], [core_write.cpp], [interpreter.cpp]
      "src/rpc/protocol.h"  ##  Used by [core_write.cpp]
      "src/rpc/server.h"  ##  Used by [core_write.cpp]
      "src/rpc/text_writer.h"  ##  Used by [block.cpp], [chainparams.cpp], [merkle.cpp], [random.cpp], [script.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp], [utilstrencodings.cpp]
      "src/script/int_serialization.h"  ##  Used by [interpreter.cpp], [limitedstack.cpp]
      "src/script/interpreter.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp]
      "src/script/limitedstack.h"  ##  Used by [limitedstack.cpp]
      "src/script/script.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [merkle.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [utilmoneystr.cpp]
      "src/script/script_error.h"  ##  Used by [interpreter.cpp]
      "src/script/script_flags.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp]
      "src/script/script_num.h"  ##  Used by [block.cpp], [chainparams.cpp], [core_write.cpp], [interpreter.cpp], [standard.cpp]
      "src/script/sigcache.h"  ##  Used by [scriptcache.cpp]
      "src/script/sign.h"     ##  Manual fix the build
      "src/script/standard.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [standard.cpp]
      "src/script/instruction.h"  ##  Used by [script.cpp]
      "src/script/instruction_iterator.h"  ##  Used by [script.cpp]
      "src/script/opcodes.h"  ##  Used by [assembler.cpp], [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [merkle.cpp], [opcodes.cpp], [script.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [utilmoneystr.cpp]
      "src/script/scriptcache.h"  ##  Used by [scriptcache.cpp]
      "src/script/sighashtype.h"  ##  Used by [base58.cpp], [chainparams.cpp], [config.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp]
      "src/script_config.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/serialize.h"  ##  Used by [amount.cpp], [assembler.cpp], [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [script.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/streams.h"  ##  Used by [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/support/allocators/secure.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [interpreter.cpp], [key.cpp]
      "src/support/allocators/zeroafterfree.h"  ##  Used by [base58.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/support/cleanse.h"  ##  Used by [base58.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [key.cpp], [lockedpool.cpp], [random.cpp], [scriptcache.cpp]
      "src/support/lockedpool.h"  ##  Used by [base58.cpp], [core_write.cpp], [dstencode.cpp], [interpreter.cpp], [key.cpp], [lockedpool.cpp]
      "src/sync.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/task.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/task_helpers.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/taskcancellation.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/threadinterrupt.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/threadpool.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/threadpoolT.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/threadpriority.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/threadsafety.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/time_locked_mempool.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/timedata.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/tinyformat.h"  ##  Used by [amount.cpp], [block.cpp], [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [script.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [util.cpp], [utilmoneystr.cpp], [utilstrencodings.cpp]
      "src/tx_mempool_info.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/txmempool.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/txn_double_spend_detector.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/txn_sending_details.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/txn_util.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/txn_validation_config.h"  ##  Used by [config.cpp], [dstencode.cpp], [interpreter.cpp]
      "src/txn_validation_data.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/txn_validation_result.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/uint256.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp]
      "src/util.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp]
      "src/utilmoneystr.h"  ##  Used by [core_write.cpp], [utilmoneystr.cpp]
      "src/utilstrencodings.h"  ##  Used by [block.cpp], [chainparams.cpp], [core_read.cpp], [core_write.cpp], [merkle.cpp], [random.cpp], [script.cpp], [standard.cpp], [transaction.cpp], [arith_uint256.cpp], [util.cpp], [utilmoneystr.cpp], [utilstrencodings.cpp]
      "src/utiltime.h"  ##  Used by [chainparams.cpp], [chainparamsbase.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [logging.cpp], [random.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [util.cpp], [utiltime.cpp]
      "src/validation.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      "src/version.h"  ##  Used by [base58.cpp], [block.cpp], [chainparams.cpp], [config.cpp], [core_read.cpp], [core_write.cpp], [dstencode.cpp], [fRequireStandard.cpp], [hash.cpp], [interpreter.cpp], [key.cpp], [limitedstack.cpp], [merkle.cpp], [pubkey.cpp], [scriptcache.cpp], [sigcache.cpp], [standard.cpp], [transaction.cpp]
      "src/versionbits.h"  ##  Used by [config.cpp], [dstencode.cpp], [fRequireStandard.cpp], [interpreter.cpp], [scriptcache.cpp]
      ## Total number of header files : 114
  )
  foreach(rel_file ${_minimal_hdr_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_HDR_FILES "${SESDK_BSV_ROOT_DIR}/${rel_file}")
  endforeach()

  set(_minimal_src_files
      "src/amount.cpp"
      "src/arith_uint256.cpp"
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
      "src/keystore.cpp"  ##  Manual fix the build
      "src/logging.cpp"
      "src/primitives/block.cpp"
      "src/primitives/transaction.cpp"
      "src/protocol_era.cpp"
      "src/pubkey.cpp"
      "src/random.cpp"
      "src/rpc/jsonwriter.cpp"
      "src/rpc/text_writer.cpp"
      "src/script/interpreter.cpp"
      "src/script/limitedstack.cpp"
      "src/script/opcodes.cpp"
      "src/script/script.cpp"
      "src/script/script_error.cpp"
      "src/script/script_num.cpp"
      "src/script/scriptcache.cpp"
      "src/script/sigcache.cpp"
      "src/script/sign.cpp"     ##  Manual fix the build
      "src/script/standard.cpp"
      "src/support/cleanse.cpp"
      "src/support/lockedpool.cpp"
      "src/taskcancellation.cpp"
      "src/util.cpp"
      "src/utilmoneystr.cpp"
      "src/utilstrencodings.cpp"
      "src/utiltime.cpp"
      ## Total number of source files : 40
  )
  foreach(rel_file ${_minimal_src_files})
    scryptAppendToGlobalSet(BSV_MINIMAL_SRC_FILES "${SESDK_BSV_ROOT_DIR}/${rel_file}")
  endforeach()
endfunction()



macro(HelpFindBSVSource)########################################################################################
  cryptFindBSVDir()
  cryptGetMinimumListBSVSource()
endmacro()

function(scryptPrintBSVSourceInfo)
  message("BSV Source info :")
  message("SESDK_BSV_ROOT_DIR  [${SESDK_BSV_ROOT_DIR}]")

  scryptPrintList("BSV_MINIMAL_HDR_FILES" "${BSV_MINIMAL_HDR_FILES}")
  scryptPrintList("BSV_MINIMAL_SRC_FILES" "${BSV_MINIMAL_SRC_FILES}")
  scryptPrintList("BSV_INCLUDE_DIRS" "${BSV_INCLUDE_DIRS}")
endfunction()
