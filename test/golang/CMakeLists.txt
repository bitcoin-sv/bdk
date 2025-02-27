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

# To have a vscode launch.json file to run and debug tests inside vscode
# Add -DCONFIGURE_GOBDK_VSCODE=ON on cmake command
if(CONFIGURE_GOBDK_VSCODE)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json.in ${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json @ONLY)
endif()

## Add preparatory command as a test, to make it executed before all other tests
## All other go test in this directory will depend to this setup
add_test(NAME test_GoBDKSetup
  COMMAND bash -c "echo '####  GoBDK test setup  ####' &&  rm -fR ${CMAKE_CURRENT_SOURCE_DIR}/vendor && ${GO_EXECUTABLE} mod tidy && ${GO_EXECUTABLE} mod vendor && ${GO_EXECUTABLE} clean -testcache && echo '####  End GoBDK test setup####'"
  PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

## Get all test file
file(GLOB GOLANG_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*_test.go")

## Create each test case corresponding to a test file
foreach(golang_test_file ${GOLANG_TEST_FILES})

  ## Skip building ctest for Main_test.go
  string(FIND "${golang_test_file}" "Main_test.go" IS_MAINTEST)
  if (IS_MAINTEST GREATER -1)
    continue()
  endif()

  get_filename_component(golang_test_file "${golang_test_file}" NAME)

  get_filename_component(golang_test_file_base "${golang_test_file}" NAME_WE)
  string(REPLACE "_test" "" tmp_test_name "${golang_test_file_base}")
  set(golang_test_name "test_GoBDK${tmp_test_name}")

  if(CMAKE_CONFIGURATION_TYPES)
    set(_tpost_fix d)
    add_test(NAME ${golang_test_name} COMMAND ${GO_EXECUTABLE} test Main_test.go ${golang_test_file} CONFIGURATIONS Release)
    set_tests_properties(${golang_test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" DEPENDS test_GoBDKSetup)
    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test Main_test.go ${golang_test_file} CONFIGURATIONS Debug)
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" DEPENDS test_GoBDKSetup)
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix d)
    endif()

    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test Main_test.go ${golang_test_file})
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" DEPENDS test_GoBDKSetup)
  endif()

endforeach()