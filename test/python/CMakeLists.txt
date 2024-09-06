################################################################
#  Date             15/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################

if(NOT TARGET Python::Interpreter)
  message(WARNING "Missing Python, skip building Python tests")
  return()
endif()

if(NOT (Python_FOUND AND Python_EXECUTABLE AND Python_Development_FOUND))
    message(WARNING "Unable to find Python on the system. Skip building Python tests")
    return()
endif()

## Get all test scripts
file(GLOB PYTHON_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_Py*.py")

## Create each test case corresponding to a test file
foreach(python_test_file ${PYTHON_TEST_FILES})
  get_filename_component(python_test_name "${python_test_file}" NAME_WE)

  if(CMAKE_CONFIGURATION_TYPES)
    set(_tpost_fix d)
    add_test(NAME ${python_test_name} COMMAND  ${Python_EXECUTABLE} -m pytest -s ${python_test_file} -o junit_suite_name=${python_test_name} --junitxml=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${python_test_name}.xml --module_dir=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} CONFIGURATIONS Release)
    ## To be able to run python on 2 configurations platform type, user need to add to PATH the location where python_d is
    add_test(NAME ${python_test_name}${_tpost_fix} COMMAND  python_d -m pytest -s ${python_test_file} -o junit_suite_name=${python_test_name}${_tpost_fix} --junitxml=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/${python_test_name}${_tpost_fix}.xml --module_dir=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG} CONFIGURATIONS Debug)
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix d)
    endif()
    add_test(NAME ${python_test_name}${_tpost_fix} COMMAND ${Python_EXECUTABLE} -m pytest -s ${python_test_file} -o junit_suite_name=${python_test_name}${_tpost_fix} --junitxml=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${python_test_name}${_tpost_fix}.xml --module_dir=${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endif()

endforeach()

