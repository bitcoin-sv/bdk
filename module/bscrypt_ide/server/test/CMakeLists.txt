#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if(NOT Boost_FOUND)
  message(STATUS "SCRYPT WARNS : Boost was not found on your system, unit tests will not be built")
  return()
endif()

macro(bscrypt_add_unit_test _targetName)
  if(NOT TARGET ${_targetName})
      message(FATAL_ERROR " [${_targetName}] is not a TARGET")
  endif()
  scryptLinkTargetToBoost(${_targetName} Boost::unit_test_framework)
  set_target_properties(${_targetName} PROPERTIES  FOLDER "module/bscrypt_ide/server/test" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

  if(CMAKE_CONFIGURATION_TYPES)
    set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
    ## Note at the moment, on Windows Py Module can be loaded only in release build. There are problem in debug build
    add_test(NAME ${_targetName} COMMAND $<TARGET_FILE:${_targetName}> --report_format=HRF --report_level=detailed --log_format=JUNIT --log_sink=$<TARGET_FILE:${_targetName}>.xml CONFIGURATIONS Release)
    add_test(NAME ${_targetName}${_tpost_fix} COMMAND $<TARGET_FILE:${_targetName}> --report_format=HRF --report_level=detailed --log_format=JUNIT --log_sink=$<TARGET_FILE:${_targetName}>.xml CONFIGURATIONS Debug)
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
    endif()
    add_test(NAME ${_targetName}${_tpost_fix} COMMAND $<TARGET_FILE:${_targetName}> --report_format=HRF --report_level=detailed --log_format=JUNIT --log_sink=$<TARGET_FILE:${_targetName}>.xml)
  endif()
endmacro()

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################
add_executable(test_http_msg "${CMAKE_CURRENT_SOURCE_DIR}/test_http_msg.cpp")

target_link_libraries(test_http_msg http_msg)
bscrypt_add_unit_test(test_http_msg)
