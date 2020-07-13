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

set(_PY_MODULE_NAME PySESDK)
set(_PY_MODULE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/PySESDK.cpp)

Python_add_library(${_PY_MODULE_NAME} ${_PY_MODULE_SOURCE_FILES})
target_link_libraries(${_PY_MODULE_NAME} PRIVATE scrypt_core OpenSSL::Crypto OpenSSL::SSL)
if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES FOLDER "module/python" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_PY_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_PY_MODULE_NAME} DESTINATION "lib" COMPONENT PyModules)
