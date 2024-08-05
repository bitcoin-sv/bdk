#################################################################
#  Date             24/04/2019                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################

if(NOT (Python_FOUND AND Python_EXECUTABLE AND Python_Development_FOUND))
    message(WARNING "Unable to find Python on the system. Skip building Python module")
    return()
endif()

#### Python module version as increment of core version
createIncrementVersion(SESDK_PYTHON_VERSION_MAJOR ${SESDK_CORE_VERSION_MAJOR} 0 "script engine python major version")
createIncrementVersion(SESDK_PYTHON_VERSION_MINOR ${SESDK_CORE_VERSION_MINOR} 1 "script engine python minor version")
createIncrementVersion(SESDK_PYTHON_VERSION_PATCH ${SESDK_CORE_VERSION_PATCH} 0 "script engine python patch version")
set(SESDK_PYTHON_VERSION_STRING "${SESDK_PYTHON_VERSION_MAJOR}.${SESDK_PYTHON_VERSION_MINOR}.${SESDK_PYTHON_VERSION_PATCH}")

## Test maximum of global version
if(${SESDK_VERSION_STRING} LESS ${SESDK_PYTHON_VERSION_STRING})
  message("Script Engine SDK global version should be the max of all version numbers")
  message("SESDK_VERSION_STRING=${SESDK_VERSION_STRING} while SESDK_PYTHON_VERSION_STRING=${SESDK_PYTHON_VERSION_STRING}")
endif()

#### Generate version python binding #####
set(SESDK_VERSION_PYTHON_HPP_IN ${CMAKE_CURRENT_SOURCE_DIR}/version_py.hpp.in CACHE INTERNAL "Template File for python version")
set(SESDK_VERSION_PYTHON_HPP ${SESDK_GENERATED_HPP_DIR}/version_py.hpp CACHE INTERNAL "HPP File for python version")
configure_file(${SESDK_VERSION_PYTHON_HPP_IN} ${SESDK_VERSION_PYTHON_HPP})

set(_PY_MODULE_NAME PySESDK)
set(_PY_MODULE_SOURCE_FILES "${SESDK_VERSION_PYTHON_HPP}" "${CMAKE_CURRENT_SOURCE_DIR}/PySESDK.cpp")

Python_add_library(${_PY_MODULE_NAME} ${_PY_MODULE_SOURCE_FILES})
target_link_libraries(${_PY_MODULE_NAME} PRIVATE sesdk_core OpenSSL::Crypto OpenSSL::SSL)
target_include_directories(${_PY_MODULE_NAME} PUBLIC ${SESDK_GENERATED_HPP_DIR})
if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES FOLDER "module/python" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_PY_MODULE_NAME} DESTINATION "lib" COMPONENT PyModules)
