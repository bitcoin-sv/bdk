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

if(NOT SESDK_BUILD_MODULES)
  message(WARNING "Deactivated building modules. Set SESDK_BUILD_MODULES=ON to activate.")
  return()
endif()

message(STATUS "Building modules ...")

add_subdirectory(example)

if(BUILD_JAVA_BINDING_ONLY AND BUILD_PYTHON_BINDING_ONLY)
	message(FATAL_ERROR "Should not activate both BUILD_PYTHON_BINDING_ONLY[${BUILD_PYTHON_BINDING_ONLY}] and BUILD_JAVA_BINDING_ONLY[${BUILD_JAVA_BINDING_ONLY}]")
endif()

if(NOT BUILD_PYTHON_BINDING_ONLY)
	add_subdirectory(java)
else()
	message(STATUS "Activation of python has disabled building java module")
endif()

if(NOT BUILD_JAVA_BINDING_ONLY)
	add_subdirectory(python)
else()
	message(STATUS "Activation of java has disabled building python module")
endif()
