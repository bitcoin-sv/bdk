#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build secp256k1 requires locating bsv source code
if(NOT DEFINED SESDK_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by SESDK_BSV_ROOT_DIR")
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


## Backup CMAKE_C_FLAGS_RELEASE to do the hotfix
set(BACKUP_CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

## This hotfix because the code in ${BSV_ROOT}/src/secp256k1/CMakeLists.txt:175 produce a bug
## It replace -O3 with all trailing whitespace by -O2. In the case the -O3 flags is at the begining
## of the flag list, if will concatenate the -O2 with other flags, making if broken
## We hotfix here by :
##     Creating a temporary CMAKE_C_FLAGS_RELEASE specifically used for secp256k1
##     Modify this temporary flags by putting the -O3 at the end of the list flags, so the bug
##     doesn't have the change to produce
##     Restore the CMAKE_C_FLAGS_RELEASE after the secp256k1 build
if(NOT MSVC)
  string(REPLACE "-O3" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
endif()

add_subdirectory("${SESDK_BSV_ROOT_DIR}/src/secp256k1" ${CMAKE_CURRENT_BINARY_DIR}/secp256k1)

## Restore CMAKE_C_FLAGS_RELEASE after the hotfix for secp256k1 build
set(CMAKE_C_FLAGS_RELEASE "{BACKUP_CMAKE_C_FLAGS_RELEASE}")

