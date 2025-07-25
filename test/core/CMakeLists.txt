#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindBoostHelper)
HelpFindBoost(thread filesystem program_options chrono)
foreach(comp IN LISTS "filesystem" "chrono" "thread" "program_options")
    if(NOT TARGET Boost::${comp})
        message(FATAL "Boost::${comp} is required but not found")
    endif()
endforeach()

## Test setting requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

if(NOT Boost_FOUND)
  message(WARNING "Unable to find boost for C++ unit test, skip testing core")
  return()
endif()

include(FindPythonHelper)
HelpFindPython()
#bdkPrintPythonInfo()#Debug Log

if(NOT (Python_FOUND AND Python_EXECUTABLE AND Python_Development_FOUND))
    message(WARNING "Unable to find Python on the system which is required to generate test files")
    return()
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

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################

message(status "File ${BDK_BSV_ROOT_DIR}/src/logging.cpp initially included in core, but as it depend to Boost::filesytem")
message(status "we remove it from core and put it where it is needed only")

add_executable(test_bsv
               "${BDK_BSV_TEST_ROOT}/jsonutil.h"
               "${BDK_BSV_TEST_ROOT}/scriptflags.h"
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
               ## These two files below are for dependencies resolve
               "${BDK_BSV_ROOT_DIR}/src/script/sigcache.cpp"
               "${BDK_BSV_ROOT_DIR}/src/logging.cpp"
               ## TODO
               #"${BDK_BSV_TEST_ROOT}/opcode_tests.cpp
)

target_link_libraries(test_bsv PRIVATE bdk_core univalue OpenSSL::Crypto OpenSSL::SSL ${CMAKE_DL_LIBS}  ${CMAKE_THREAD_LIBS_INIT})
bdkLinkTargetToBoost(test_bsv Boost::thread Boost::chrono Boost::filesystem Boost::program_options)
if(WIN32)
  target_link_libraries(test_bsv PRIVATE Crypt32.lib Ws2_32)
endif()
target_include_directories(test_bsv PRIVATE "${BDK_BSV_TEST_ROOT}")
bdk_add_unit_test(test_bsv)

####################################################################################
## Test for the bdk core part ######################################################
message("Building BDK core tests ...")

add_executable(test_assembler "${CMAKE_CURRENT_SOURCE_DIR}/test_assembler.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(test_assembler PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_assembler)

add_executable(test_extended_tx "${CMAKE_CURRENT_SOURCE_DIR}/test_extended_tx.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(test_extended_tx PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_extended_tx)

add_executable(test_script_error "${CMAKE_CURRENT_SOURCE_DIR}/test_script_error.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(test_script_error PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_script_error)

add_executable(test_scriptengine "${CMAKE_CURRENT_SOURCE_DIR}/test_scriptengine.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(test_scriptengine PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_scriptengine)

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(test_bsv PROPERTIES FOLDER "test/core/bsv" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_assembler PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_extended_tx PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_script_error PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_scriptengine PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(test_bsv PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_assembler PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_extended_tx PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_script_error PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_scriptengine PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()