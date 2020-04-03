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
  ##   If inside the bscrypt project, there are a directory '${CMAKE_SOURCE_DIR}/bitcoin-sv', then it will be used      ## jenkins build
  ##   If alongside the bscrypt project, there are a directory ${CMAKE_SOURCE_DIR}/../bitcoin-sv, then it will be used  ## local development
  ##   If non of them exist, then it will be clone from source into '${CMAKE_SOURCE_DIR}/bitcoin-sv'
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

  if(EXISTS "${CMAKE_SOURCE_DIR}/bitcoin-sv")
    set(SCRYPT_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/bitcoin-sv" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  if(EXISTS "${CMAKE_SOURCE_DIR}/../bitcoin-sv")
    get_filename_component(_ABS_PATH "${CMAKE_SOURCE_DIR}/../bitcoin-sv" ABSOLUTE)
    set(SCRYPT_BSV_ROOT_DIR "${_ABS_PATH}" CACHE PATH "Root directory for BSV source code" FORCE)
    _checkprint_SCRYPT_BSV_ROOT_DIR()
    return()
  endif()

  message(STATUS "Unable to find bsv source code on local machine. It will be clone to [${CMAKE_SOURCE_DIR}/bitcoin-sv]")
  execute_process(
    COMMAND git clone https://github.com/bitcoin-sv/bitcoin-sv.git
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/bitcoin-sv")
    message(FATAL_ERROR "Unable to clone [https://github.com/bitcoin-sv/bitcoin-sv.git] into [${CMAKE_SOURCE_DIR}/bitcoin-sv]. Check internet connection")
  else()
    message(STATUS "Successfully cloned [https://github.com/bitcoin-sv/bitcoin-sv.git] into [${CMAKE_SOURCE_DIR}/bitcoin-sv]")
    set(SCRYPT_BSV_ROOT_DIR "${CMAKE_SOURCE_DIR}/bitcoin-sv" CACHE PATH "Root directory for BSV source code" FORCE)
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
      "src/script/scriptcache.h"  ##  Used by test init
      "src/script/sigcache.h"  ##  Used by test init
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
    scryptAppendToGlobalSet(BSV_MINIMAL_HDR_FILES "${SCRYPT_BSV_ROOT_DIR}/${rel_file}")
  endforeach()

  set(_minimal_src_files
      "src/amount.cpp"
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
      "src/script/script_error.cpp"
      "src/script/script_num.cpp"
      "src/script/scriptcache.cpp"
      "src/script/sigcache.cpp"
      "src/script/standard.cpp"
      "src/support/cleanse.cpp"
      "src/support/lockedpool.cpp"
      "src/uint256.cpp"
      "src/util.cpp"
      "src/utilmoneystr.cpp"
      "src/utilstrencodings.cpp"
      "src/utiltime.cpp"
      ## Total number of source files : 40
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
