#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Test setting requires locating bsv source code
if(NOT DEFINED SESDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by SESDK_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${SESDK_BSV_ROOT_DIR}/sv/src/test/CMakeLists.txt with slightl modifications
#######################################################################
message("Including tests from ${SESDK_BSV_ROOT_DIR} ...")

set(SESDK_BSV_TEST_ROOT "${SESDK_BSV_ROOT_DIR}/src/test")
set(SESDK_BSV_TESTSUITE_MODULE "${SESDK_BSV_ROOT_DIR}/cmake/modules/TestSuite.cmake")
set(SESDK_BSV_TEST_HEADER_GENERATOR "${SESDK_BSV_TEST_ROOT}/data/generate_header.py")

function(gen_json_header NAME)
  set(HEADERS "")
  foreach(f ${ARGN})
    set(_h_file "${SESDK_GENERATED_HPP_DIR}/${f}.h")
    set(_json_file "${SESDK_BSV_TEST_ROOT}/${f}")
    # Get the proper name for the test variable.
    get_filename_component(TEST_NAME ${f} NAME_WE)

    add_custom_command(
      OUTPUT ${_h_file}
      COMMAND ${Python_EXECUTABLE}
      ARGS "${SESDK_BSV_TEST_HEADER_GENERATOR}" "${TEST_NAME}" "${_json_file}" > ${_h_file}
      MAIN_DEPENDENCY ${_json_file}
      DEPENDS "${SESDK_BSV_TEST_HEADER_GENERATOR}"
      VERBATIM
    )

    list(APPEND HEADERS ${_h_file})
  endforeach(f)
  set(${NAME} "${HEADERS}" PARENT_SCOPE)
endfunction()

## Generate directory containing generated json header file
file(MAKE_DIRECTORY "${SESDK_GENERATED_HPP_DIR}/data")

gen_json_header(JSON_HEADERS
  data/script_tests.json
  data/base58_keys_valid.json
  data/base58_encode_decode.json
  data/base58_keys_invalid.json
  data/tx_invalid.json
  data/tx_valid.json
  data/sighash.json
)
message(STATUS "JSON_HEADERS generated for BSV tests")
foreach(_f ${JSON_HEADERS})
  if(SESDK_LOG_BSV_FILES)
    message("       [${_f}]")
  endif()
endforeach()

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################
add_executable(test_core
               "${SESDK_BSV_TEST_ROOT}/jsonutil.h"
               "${SESDK_BSV_TEST_ROOT}/scriptflags.h"
               "${SESDK_BSV_TEST_ROOT}/sigutil.h"
               "${SESDK_BSV_TEST_ROOT}/jsonutil.cpp"
               "${SESDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${SESDK_BSV_TEST_ROOT}/sigutil.cpp"
               ${JSON_HEADERS}
               "${CMAKE_CURRENT_SOURCE_DIR}/script_tests_modified.cpp"
               "${SESDK_BSV_TEST_ROOT}/bn_op_tests.cpp"
#               "${SESDK_BSV_TEST_ROOT}/config_tests.cpp"
               "${SESDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${SESDK_BSV_TEST_ROOT}/scriptnum_tests.cpp"
               ## TODO
               #"${SESDK_BSV_TEST_ROOT}/opcode_tests.cpp
)

target_link_libraries(test_core ${CMAKE_DL_LIBS} sesdk_core OpenSSL::Crypto OpenSSL::SSL)
if(WIN32)
  target_link_libraries(test_core Crypt32.lib Ws2_32)
endif()
target_include_directories(test_core PRIVATE "${SESDK_BSV_TEST_ROOT}")
sesdk_add_unit_test(test_core)
