#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################


## Modules are build based on (link to) scrypt-core
## They are considered as orthogonal extension applications, libraries that extends script's functionalities
##
## Modules should depend ONLY scrypt-core, and NOT each other

if(NOT SCRYPT_BUILD_MODULES)
  message(WARNING "Deactivated building modules. Set SCRYPT_BUILD_MODULES=ON to activate.")
  return()
endif()

message(STATUS "Building modules ...")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/example)
add_subdirectory(ScriptEngineIF)

if(BSCRYPT_BUILD_JAVA)
  add_subdirectory(java)
endif()

if(BSCRYPT_BUILD_PYTHON)
  add_subdirectory(python)
endif()