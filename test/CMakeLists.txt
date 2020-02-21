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

macro(scrypt_add_unit_test _targetName)
  if(NOT TARGET ${_targetName})
      message(FATAL_ERROR " [${_targetName}] is not a TARGET")
  endif()
  scryptLinkTargetToBoost(${_targetName} Boost::unit_test_framework)
  set_target_properties(${_targetName} PROPERTIES  FOLDER "tests/unit" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

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

include("${CMAKE_CURRENT_SOURCE_DIR}/setting-test.cmake")

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################
add_executable(test_scrypt_core
               "${SCRYPT_BSV_TEST_ROOT}/jsonutil.h"
               "${SCRYPT_BSV_TEST_ROOT}/scriptflags.h"
               "${SCRYPT_BSV_TEST_ROOT}/sigutil.h"
               "${SCRYPT_BSV_TEST_ROOT}/jsonutil.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/scriptflags.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/sigutil.cpp"
               ${JSON_HEADERS}
               "${CMAKE_CURRENT_SOURCE_DIR}/script_tests_modified.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/bn_op_tests.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/scriptflags.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/scriptnum_tests.cpp"
               ## TODO
               #"${SCRYPT_BSV_TEST_ROOT}/opcode_tests.cpp
)

target_link_libraries(test_scrypt_core ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
if(WIN32)
  target_link_libraries(test_scrypt_core Crypt32.lib Ws2_32)
endif()
target_include_directories(test_scrypt_core PRIVATE "${SCRYPT_BSV_TEST_ROOT}")
scrypt_add_unit_test(test_scrypt_core)

####################################################################################################################
## Test extra c++ source files in core (wrapper to bsv code) #######################################################
add_executable(test_core_extra
               "${SCRYPT_BSV_TEST_ROOT}/jsonutil.h"
               "${SCRYPT_BSV_TEST_ROOT}/scriptflags.h"
               "${SCRYPT_BSV_TEST_ROOT}/sigutil.h"
               "${SCRYPT_BSV_TEST_ROOT}/jsonutil.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/scriptflags.cpp"
               "${SCRYPT_BSV_TEST_ROOT}/sigutil.cpp"
               ${JSON_HEADERS}
               "${CMAKE_CURRENT_SOURCE_DIR}/test_core_FlagMap.cpp"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_core_ScriptHelper.cpp"
)
target_link_libraries(test_core_extra ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
scrypt_add_unit_test(test_core_extra)
