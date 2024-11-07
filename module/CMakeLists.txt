#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################


## Modules are build based on (link to) bdk-core
## They are considered as orthogonal extension applications, libraries that extends script's functionalities
##
## Modules should depend ONLY bdk-core, and NOT each other

if(NOT BDK_BUILD_MODULES)
  message(WARNING "Deactivated building modules. Set BDK_BUILD_MODULES=ON to activate.")
  return()
endif()

message(STATUS "Building modules ...")

add_subdirectory(example)

if(BUILD_MODULE_GOLANG)
	add_subdirectory(gobdk/bdkcgo)
else()
	message(STATUS "Building Golang module has been disabled")
endif()

if(BUILD_MODULE_JAVA)
	add_subdirectory(java)
else()
	message(STATUS "Building Java module has been disabled")
endif()

if(BUILD_MODULE_PYTHON)
	add_subdirectory(python)
else()
	message(STATUS "Building Python module has been disabled")
endif()
