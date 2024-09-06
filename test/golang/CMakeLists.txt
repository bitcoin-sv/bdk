################################################################
#  Date             06/09/2024                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################

find_program(GO_EXECUTABLE NAMES go)

if(NOT GO_EXECUTABLE)
  message(WARNING "Missing go, skip building Golang tests")
  return()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json.in ${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json @ONLY)

## Get all test file
file(GLOB GOLANG_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*_test.go")

## Create each test case corresponding to a test file
foreach(golang_test_file ${GOLANG_TEST_FILES})
  get_filename_component(golang_test_file "${golang_test_file}" NAME)

  get_filename_component(golang_test_file_base "${golang_test_file}" NAME_WE)
  string(REPLACE "_test" "" tmp_test_name "${golang_test_file_base}")
  set(golang_test_name "test_GoBDK${tmp_test_name}")

  if(CMAKE_CONFIGURATION_TYPES)
    set(_tpost_fix d)
    add_test(NAME ${golang_test_name} COMMAND ${GO_EXECUTABLE} test ${golang_test_file} CONFIGURATIONS Release)
    set_tests_properties(${golang_test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/x64/release")
    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test ${golang_test_file} CONFIGURATIONS Debug)
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/x64/debug")
  else()
    set(_builddir release)
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix d)
      set(_builddir debug)
    endif()
    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test ${golang_test_file})
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/x64/${_builddir};LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/x64/${_builddir}")
  endif()

endforeach()

