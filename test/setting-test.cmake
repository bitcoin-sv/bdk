#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Test setting requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_SOURCE_ROOT)#
  message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_SOURCE_ROOT")
endif()
# This file is mainly copied from ${SCRYPT_BSV_SOURCE_ROOT}/sv/src/test/CMakeLists.txt with slightl modifications
#######################################################################

if(NOT TARGET Python::Interpreter)
  message(FATAL_ERROR "Python Interpreter is required to build tests")
endif()

set(SCRYPT_BSV_TEST_ROOT "${SCRYPT_BSV_SOURCE_ROOT}/src/test")
set(SCRYPT_BSV_TESTSUITE_MODULE "${SCRYPT_BSV_SOURCE_ROOT}/cmake/modules/TestSuite.cmake")
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