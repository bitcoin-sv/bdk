#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build secp256k1 requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

#######################################################################

## The MSVC build for secp256k1 has set for coverage build, which we don't need that anyway
## This setting make the build broken, cmake complain that CMAKE_MODULE_LINKER_FLAGS_COVERAGE
## is required but not set. We just set it here to satisfy cmake, that'd fix the msvc cmake run
if(MSVC)
  set(CMAKE_MODULE_LINKER_FLAGS_COVERAGE "${CMAKE_MODULE_LINKER_FLAGS_COVERAGE} --coverage" CACHE STRING
    "Flags used by the shared libraries linker during \"Coverage\" builds."
    FORCE
  )
  mark_as_advanced(CMAKE_MODULE_LINKER_FLAGS_COVERAGE)
endif()

# We can disable the test building for secp256k1 here as it is not our job
set(SECP256K1_BUILD_TESTS OFF)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF)

# Enable architecture-specific optimizations for secp256k1:
# - x86_64: hand-optimized assembly for field/scalar ops + -march=native (AVX2, etc.)
# - ARM64:  generic C with __int128 + -mcpu=native (Apple M-series/Neoverse tuning)
# Also override secp256k1's default -O2 with -O3 for maximum throughput.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(SECP256K1_ASM "x86_64" CACHE STRING "" FORCE)
  set(SECP256K1_APPEND_CFLAGS "-march=native -O3" CACHE STRING "" FORCE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(SECP256K1_ASM "OFF" CACHE STRING "" FORCE)
  set(SECP256K1_APPEND_CFLAGS "-mcpu=native -O3" CACHE STRING "" FORCE)
else()
  set(SECP256K1_APPEND_CFLAGS "-O3" CACHE STRING "" FORCE)
endif()

add_subdirectory("${BDK_BSV_ROOT_DIR}/src/secp256k1" ${CMAKE_CURRENT_BINARY_DIR}/secp256k1)

## Set the IDE Folder to the created targets for secp256k1 to the right place #########
set(_targetList bench bench_ecmult bench_internal secp256k1 secp256k1_precomputed)
foreach(_t ${_targetList})
  if(TARGET ${_t})
    set_target_properties(${_t} PROPERTIES  FOLDER "core/secp256k1" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
  endif()
endforeach()

set(_BSV_SECP256K1_INSTALL_HDR_FILES
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_ellswift.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_preallocated.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_schnorrsig.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_ecdh.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_extrakeys.h"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include/secp256k1_recovery.h"
)

#######################################################################################################################
## Install secp256k1 the *.h header files should be kept as secp256k1 structure    ####################################
set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include files for secp256k1\n")
foreach(_secp256k1_pubhdr ${_BSV_SECP256K1_INSTALL_HDR_FILES})
  install(FILES ${_secp256k1_pubhdr} DESTINATION "include/secp256k1/include" COMPONENT secp256k1)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_secp256k1_pubhdr} NAME)
  set(_install_rl "secp256k1//include/${_f}")
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_install_rl}\"\n")
endforeach()
install(TARGETS secp256k1 DESTINATION "lib" COMPONENT secp256k1)
