#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if(NOT Boost_FOUND)
  message(WARNING "Unable to find boost for C++ unit test, skip testing core")
  return()
endif()

## Test setting requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${BDK_BSV_ROOT_DIR}/sv/src/test/CMakeLists.txt with slightl modifications
#######################################################################
message("Building BSV tests from ${BDK_BSV_ROOT_DIR} ...")

set(BDK_BSV_TEST_ROOT "${BDK_BSV_ROOT_DIR}/src/test")
set(BDK_BSV_TESTSUITE_MODULE "${BDK_BSV_ROOT_DIR}/cmake/modules/TestSuite.cmake")
set(BDK_BSV_TEST_HEADER_GENERATOR "${BDK_BSV_TEST_ROOT}/data/generate_header.py")

function(gen_json_header NAME)
  set(HEADERS "")
  foreach(f ${ARGN})
    set(_h_file "${BDK_GENERATED_HPP_DIR}/${f}.h")
    set(_json_file "${BDK_BSV_TEST_ROOT}/${f}")
    # Get the proper name for the test variable.
    get_filename_component(TEST_NAME ${f} NAME_WE)

    add_custom_command(
      OUTPUT ${_h_file}
      COMMAND ${Python_EXECUTABLE}
      ARGS "${BDK_BSV_TEST_HEADER_GENERATOR}" "${TEST_NAME}" "${_json_file}" > ${_h_file}
      MAIN_DEPENDENCY ${_json_file}
      DEPENDS "${BDK_BSV_TEST_HEADER_GENERATOR}"
      VERBATIM
    )

    list(APPEND HEADERS ${_h_file})
  endforeach(f)
  set(${NAME} "${HEADERS}" PARENT_SCOPE)
endfunction()

## Generate directory containing generated json header file
file(MAKE_DIRECTORY "${BDK_GENERATED_HPP_DIR}/data")

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
  if(BDK_LOG_BSV_FILES)
    message("       [${_f}]")
  endif()
endforeach()



#######################################################################################
#######################################################################################
# Fix for gcc13 for as the code bsv 1.0.13 broke it
# The file "${BDK_BSV_TEST_ROOT}/scriptflags.h" broke the build because it used the uint32_t without #include <cstdint>. gcc13 don't like it, which is the code bsv 1.0.13
# What we'll do :
# We don't include it as it is, but we copy it to our generated directory, then modify it
# Then we include the modified version

set(HOTFIX_DIRECTORY_SCRIPTFLAGS "${BDK_GENERATED_HPP_DIR}/hotfix-scriptflags")
set(BROKEN_SCRIPTFLAGS_H "${BDK_BSV_TEST_ROOT}/scriptflags.h")
set(MODIFIED_SCRIPTFLAGS_H "${HOTFIX_DIRECTORY_SCRIPTFLAGS}/test/scriptflags.h")
configure_file("${BROKEN_SCRIPTFLAGS_H}" "${MODIFIED_SCRIPTFLAGS_H}" COPYONLY)

message(STATUS "BDK Warning : Hot fix gcc 13")
message(STATUS "BDK Warning : The build fails because in file src/test/scriptflags.h, it use uint32_t but doesn't include <cstdint>")
message(STATUS "BDK Warning : We \"hot-fix\" that by copying this file to our generated directory, then modify it")

# Read the file content and prepare lines to replace
file(READ "${MODIFIED_SCRIPTFLAGS_H}" FILE_CONTENTS)
set(LINE_TO_INSERT "\n#include <cstdint>\n")
set(INSERT_AFTER "#include <string>")

# Modify the content: Insert the new line after a specific line
string(REPLACE "${INSERT_AFTER}" "${INSERT_AFTER}${LINE_TO_INSERT}" MODIFIED_CONTENTS "${FILE_CONTENTS}")

# Write the modified content back to the file
file(WRITE "${MODIFIED_SCRIPTFLAGS_H}" "${MODIFIED_CONTENTS}")
#######################################################################################

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################
add_executable(test_bsv
               "${BDK_BSV_TEST_ROOT}/jsonutil.h"
               #"${BDK_BSV_TEST_ROOT}/scriptflags.h" This file is replaced by the hotfix modified file
               "${MODIFIED_SCRIPTFLAGS_H}"
               "${BDK_BSV_TEST_ROOT}/sigutil.h"
               "${BDK_BSV_TEST_ROOT}/jsonutil.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${BDK_BSV_TEST_ROOT}/sigutil.cpp"
               ${JSON_HEADERS}
               "${CMAKE_CURRENT_SOURCE_DIR}/script_tests_modified.cpp"
               "${BDK_BSV_TEST_ROOT}/bn_op_tests.cpp"
#               "${BDK_BSV_TEST_ROOT}/config_tests.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptnum_tests.cpp"
               ## TODO
               #"${BDK_BSV_TEST_ROOT}/opcode_tests.cpp
)

target_link_libraries(test_bsv ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
if(WIN32)
  target_link_libraries(test_bsv Crypt32.lib Ws2_32)
endif()
target_include_directories(test_bsv PRIVATE "${BDK_BSV_TEST_ROOT}" "${HOTFIX_DIRECTORY_SCRIPTFLAGS}")
bdk_add_unit_test(test_bsv)

####################################################################################
## Test for the bdk core part ######################################################
message("Building BDK core tests ...")

add_executable(test_interpreter "${CMAKE_CURRENT_SOURCE_DIR}/test_interpreter.cpp")
target_link_libraries(test_interpreter ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
bdk_add_unit_test(test_interpreter)

add_executable(test_assembler "${CMAKE_CURRENT_SOURCE_DIR}/test_assembler.cpp")
target_link_libraries(test_assembler ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
bdk_add_unit_test(test_assembler)

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(test_bsv PROPERTIES FOLDER "test/core/bsv" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_interpreter PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_assembler PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(test_bsv PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_interpreter PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_assembler PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()