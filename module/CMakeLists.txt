#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if(SCRYPT_BUILD_MODULES)
  if(SCRYPT_BUILD_IDE)# add -DSCRYPT_BUILD_IDE=ON to activate build IDE
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide)
  endif()

else()
  message(WARNING "Deactivated building modules. Set SCRYPT_BUILD_MODULES=ON to activate.")
endif()