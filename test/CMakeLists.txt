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

if(NOT TARGET Python::Interpreter)
  message(FATAL_ERROR "Python Interpreter is required to run tests on CI")
endif()

macro(scrypt_add_unit_test _targetName)
  if(NOT TARGET ${_targetName})
      message(FATAL_ERROR " [${_targetName}] is not a TARGET")
  endif()
  scryptLinkTargetToBoost(${_targetName} Boost::unit_test_framework)
  set_target_properties(${_targetName} PROPERTIES  FOLDER "test/unit" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

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

if(SCRYPT_BUILD_BSV_TESTS)## Broken now as dependencies has been reduce to minimal
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/bsv")
endif()

add_executable(test_foo "${CMAKE_CURRENT_SOURCE_DIR}/test_foo.cpp")
target_link_libraries(test_foo ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
scrypt_add_unit_test(test_foo)

add_executable(test_scriptengineif "${CMAKE_CURRENT_SOURCE_DIR}/testSEIF.cpp")
target_link_libraries(test_scriptengineif ${CMAKE_DL_LIBS} ScriptEngineIF OpenSSL::Crypto OpenSSL::SSL)
scrypt_add_unit_test(test_scriptengineif)
if(BSCRYPT_BUILD_JAVA)
    add_subdirectory(java)
endif()

add_subdirectory(python)
