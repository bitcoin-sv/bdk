#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindBoostHelper)
HelpFindBoost(unit_test_framework)
if (NOT TARGET Boost::unit_test_framework)
  message(FATAL_ERROR "Boost::unit_test_framework is required but not found")
endif()

## Registers <target><suffix> running ARGN, as a Release and a Debug entry in a
## multi-config build; sets _bdk_test_names to the entries made.
macro(_bdk_add_boost_test _targetName _suffix)
  set(_bdk_test_args --report_format=HRF --report_level=detailed --log_format=JUNIT --log_sink=$<TARGET_FILE:${_targetName}>${_suffix}.xml ${ARGN})
  if(CMAKE_CONFIGURATION_TYPES)
    ## Note at the moment, on Windows Py Module can be loaded only in release build. There are problem in debug build
    add_test(NAME ${_targetName}${_suffix} COMMAND $<TARGET_FILE:${_targetName}> ${_bdk_test_args} CONFIGURATIONS Release)
    add_test(NAME ${_targetName}${CMAKE_DEBUG_POSTFIX}${_suffix} COMMAND $<TARGET_FILE:${_targetName}> ${_bdk_test_args} CONFIGURATIONS Debug)
    set(_bdk_test_names ${_targetName}${_suffix} ${_targetName}${CMAKE_DEBUG_POSTFIX}${_suffix})
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_bdk_test_names ${_targetName}${CMAKE_DEBUG_POSTFIX}${_suffix})
    add_test(NAME ${_bdk_test_names} COMMAND $<TARGET_FILE:${_targetName}> ${_bdk_test_args})
  else()
    set(_bdk_test_names ${_targetName}${_suffix})
    add_test(NAME ${_bdk_test_names} COMMAND $<TARGET_FILE:${_targetName}> ${_bdk_test_args})
  endif()
endmacro()

macro(bdk_add_unit_test _targetName)
  if(NOT TARGET ${_targetName})
      message(FATAL_ERROR " [${_targetName}] is not a TARGET")
  endif()
  bdkLinkTargetToBoost(${_targetName} Boost::unit_test_framework)
  set_target_properties(${_targetName} PROPERTIES  FOLDER "test/unit" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
  _bdk_add_boost_test(${_targetName} "" ${ARGN})
endmacro()

## Like bdk_add_unit_test, but test cases labelled `integration` in Boost (those
## that write files or read the live system) run as a second entry,
## <target>_integration, labelled `integration` in CTest. `ctest -LE integration`
## runs only the pure unit tests, `ctest -L integration` only these.
## Boost fails the first entry if no case is unlabelled; use
## bdk_add_integration_test for such a binary.
macro(bdk_add_unit_and_integration_test _targetName)
  bdk_add_unit_test(${_targetName} --run_test=!@integration)
  _bdk_add_boost_test(${_targetName} _integration --run_test=@integration)
  set_tests_properties(${_bdk_test_names} PROPERTIES LABELS integration)
endmacro()

## Like bdk_add_unit_test, for a binary whose every test case writes files or
## reads the live system: its one entry is labelled `integration` in CTest.
macro(bdk_add_integration_test _targetName)
  bdk_add_unit_test(${_targetName})
  set_tests_properties(${_bdk_test_names} PROPERTIES LABELS integration)
endmacro()

if(BDK_BUILD_CORE_TESTS)## Broken now as dependencies has been reduce to minimal
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/core")
endif()

## Test binding modules
if(BUILD_MODULE_GOLANG AND NOT MSVC)
  add_subdirectory(golang)
  ## The cgo shim itself, exercised natively against the static-GoBDK target
  add_subdirectory(gobdkshim)
else()
  message(STATUS "Testing Golang module has been disabled")
endif()

if(BUILD_MODULE_RUST AND NOT MSVC)
  add_subdirectory(rust)
else()
  message(STATUS "Testing Rust module has been disabled")
endif()