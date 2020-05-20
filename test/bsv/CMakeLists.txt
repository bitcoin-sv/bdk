#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Test setting requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${SCRYPT_BSV_ROOT_DIR}/sv/src/test/CMakeLists.txt with slightl modifications
#######################################################################
message("Including tests from ${SCRYPT_BSV_ROOT_DIR} ...")

set(SCRYPT_BSV_TEST_ROOT "${SCRYPT_BSV_ROOT_DIR}/src/test")
set(SCRYPT_BSV_TESTSUITE_MODULE "${SCRYPT_BSV_ROOT_DIR}/cmake/modules/TestSuite.cmake")
set(SCRYPT_BSV_TEST_HEADER_GENERATOR "${SCRYPT_BSV_TEST_ROOT}/data/generate_header.py")

function(gen_json_header NAME)
  set(HEADERS "")
  foreach(f ${ARGN})
    set(_h_file "${SCRYPT_GENERATED_HPP_DIR}/${f}.h")
    set(_json_file "${SCRYPT_BSV_TEST_ROOT}/${f}")
    # Get the proper name for the test variable.
    get_filename_component(TEST_NAME ${f} NAME_WE)

    add_custom_command(
      OUTPUT ${_h_file}
      COMMAND ${Python_EXECUTABLE}
      ARGS "${SCRYPT_BSV_TEST_HEADER_GENERATOR}" "${TEST_NAME}" "${_json_file}" > ${_h_file}
      MAIN_DEPENDENCY ${_json_file}
      DEPENDS "${SCRYPT_BSV_TEST_HEADER_GENERATOR}"
      VERBATIM
    )

    list(APPEND HEADERS ${_h_file})
  endforeach(f)
  set(${NAME} "${HEADERS}" PARENT_SCOPE)
endfunction()

## Generate directory containing generated json header file
file(MAKE_DIRECTORY "${SCRYPT_GENERATED_HPP_DIR}/data")

gen_json_header(JSON_HEADERS
  data/script_tests.json
  data/base58_keys_valid.json
  data/base58_encode_decode.json
  data/base58_keys_invalid.json
  data/tx_invalid.json
  data/tx_valid.json
  data/sighash.json
)
message("JSON_HEADERS[${JSON_HEADERS}]")



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