#################################################################
#  Date             17/06/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

# cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt -DSCRYPT_BUILD_MISC_ONLY=ON

if(NOT SCRYPT_BUILD_MISC_ONLY)
  message(STATUS "Skip building misc SCRYPT_BUILD_MISC_ONLY=${SCRYPT_BUILD_MISC_ONLY}")
  return()
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/hll_investigation)
