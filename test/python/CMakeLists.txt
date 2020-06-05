################################################################
#  Date             15/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################

message (PYTHONTESTS " PYTHON TESTS ")
message ( CMAKE_CURRENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(FTEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
## Get all test scripts
file(GLOB _FUNCTIONAL_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.py")

## Create each test case corresponding to a test file
foreach(_ftest_file ${_FUNCTIONAL_TEST_FILES})
  get_filename_component(_ftest_file_name "${_ftest_file}" NAME_WE)
  string(REGEX REPLACE "test_" "py_test_" _test_name ${_ftest_file_name})

  message (path " ${_test_name} COMMAND ${Python_EXECUTABLE} pytest ${_ftest_file}")
  #add_test(NAME ${_test_name} COMMAND ${Python_EXECUTABLE} -m pytest ${_ftest_file} -s ${_ftest_file} -o junit_suite_name=${_test_name}${_tpost_fix})

  if(CMAKE_CONFIGURATION_TYPES)
  set(_tpost_fix d)
    ## Note at the moment, on Windows Py Module can be loaded only in release build. There are problem in debug build
    add_test(NAME ${_test_name} COMMAND  ${Python_EXECUTABLE} -m pytest -s ${_ftest_file} -o junit_suite_name=${_test_name} --rootdir=${CMAKE_CURRENT_SOURCE_DIR} CONFIGURATIONS Release)
    ## To be able to run python on 2 configurations platform type, user need to add to PATH the location where python_d is
    add_test(NAME ${_test_name}${_tpost_fix} COMMAND  python_d -m pytest -s ${_ftest_file} -o junit_suite_name=${_test_name}${_tpost_fix} --rootdir=${CMAKE_CURRENT_SOURCE_DIR} --junitxml=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/${_test_name}${_tpost_fix}.xml CONFIGURATIONS Debug)
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix d)
    endif()
    add_test(NAME ${_test_name}${_tpost_fix} COMMAND ${Python_EXECUTABLE} -m pytest -s ${_ftest_file} -o junit_suite_name=${_test_name}${_tpost_fix} --rootdir=${CMAKE_CURRENT_SOURCE_DIR} --module_dir=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
  endif()

endforeach()

