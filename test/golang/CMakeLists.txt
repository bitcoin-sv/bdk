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


message(INFO "Preparing go vendor for testing GoBDK")
execute_process(
  COMMAND ${GO_EXECUTABLE} mod vendor
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

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
    set_tests_properties(${golang_test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${BDK_BSV_ROOT_DIR}/src -I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/x64/release")
    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test ${golang_test_file} CONFIGURATIONS Debug)
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${BDK_BSV_ROOT_DIR}/src -I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/x64/debug")
  else()
    set(_builddir release)
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix d)
      set(_builddir debug)
    endif()

    set(_buildpath "${CMAKE_BINARY_DIR}/x64/${_builddir}")
    set(_cgo_ldflags "-L${_buildpath}")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      # On MacOS, need to add rpath to the CGO_LDFLAGS
      set(_cgo_ldflags "${_cgo_ldflags} -Wl,-rpath,${_buildpath}")
    endif()

    add_test(NAME ${golang_test_name}${_tpost_fix} COMMAND ${GO_EXECUTABLE} test ${golang_test_file})
    set_tests_properties(${golang_test_name}${_tpost_fix} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ENVIRONMENT "CGO_CFLAGS=-I${BDK_BSV_ROOT_DIR}/src -I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/module/gobdk/cgo/include;CGO_LDFLAGS=${_cgo_ldflags};LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/x64/${_builddir}")
  endif()

endforeach()

# To test memory leak while running all GoBDK tests :
#
# Build the test binary
#     CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include" CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin" go test -c -o testGoBDK
# Run the test using valgrind
#     LD_LIBRARY_PATH="${BDK_INSTALL_ROOT}/bin" valgrind --leak-check=full --show-reachable=yes --track-origins=yes --log-file=valgrind.log ./testGoBDK
# Check the file valgrind.log to see if there are any memory leaks