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
createIncrementVersion(BDK_PYTHON_VERSION_MAJOR ${BDK_CORE_VERSION_MAJOR} 0 "BDK python major version")
createIncrementVersion(BDK_PYTHON_VERSION_MINOR ${BDK_CORE_VERSION_MINOR} 1 "BDK python minor version")
createIncrementVersion(BDK_PYTHON_VERSION_PATCH ${BDK_CORE_VERSION_PATCH} 0 "BDK python patch version")
set(BDK_PYTHON_VERSION_STRING "${BDK_PYTHON_VERSION_MAJOR}.${BDK_PYTHON_VERSION_MINOR}.${BDK_PYTHON_VERSION_PATCH}")

## Test maximum of global version
if(${BDK_VERSION_STRING} LESS ${BDK_PYTHON_VERSION_STRING})
  message("Bitcoin Development Kit global version should be the max of all version numbers")
  message("BDK_VERSION_STRING=${BDK_VERSION_STRING} while BDK_PYTHON_VERSION_STRING=${BDK_PYTHON_VERSION_STRING}")
endif()

#### Generate version python binding #####
set(BDK_VERSION_PYTHON_HPP_IN ${CMAKE_CURRENT_SOURCE_DIR}/version_py.hpp.in CACHE INTERNAL "Template File for python version")
set(BDK_VERSION_PYTHON_HPP ${BDK_GENERATED_HPP_DIR}/version_py.hpp CACHE INTERNAL "HPP File for python version")
configure_file(${BDK_VERSION_PYTHON_HPP_IN} ${BDK_VERSION_PYTHON_HPP})

set(_PY_MODULE_NAME PyBDK)
set(_PY_MODULE_SOURCE_FILES "${BDK_VERSION_PYTHON_HPP}" "${CMAKE_CURRENT_SOURCE_DIR}/PyBDK.cpp")

Python_add_library(${_PY_MODULE_NAME} ${_PY_MODULE_SOURCE_FILES})
target_link_libraries(${_PY_MODULE_NAME} PRIVATE bdk_core OpenSSL::Crypto OpenSSL::SSL)
target_include_directories(${_PY_MODULE_NAME} PUBLIC ${BDK_GENERATED_HPP_DIR})
if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES FOLDER "module/python" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_PY_MODULE_NAME} DESTINATION "bin" COMPONENT PyModules)
