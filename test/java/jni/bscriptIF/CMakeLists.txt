################################################################
#  Date             07/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

find_package(Java REQUIRED)
include(UseJava)

find_jar(JUNIT_JAR NAMES junit4 PATHS $ENV{JUNIT_HOME} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})
find_jar(HAMCREST_JAR NAME hamcrest-core PATHS $ENV{JUNIT_HOME} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})

#set commands
set(CMAKE_INCLUDE_CURRENT_DIR ON)## Always include the current directory by default is off

set(JAVA_PACKAGE_NAME "jni.bscriptIF")

set(PROJECT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(SOURCE_DIRECTORY  ${PROJECT_DIRECTORY}/module/java/jni/bscriptIF)
set(TEST_DIRECTORY  ${PROJECT_DIRECTORY}/test/java/jni/bscriptIF)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BIN_DIRECTORY  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
    elseif()
        set(BIN_DIRECTORY  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
    endif(CMAKE_BUILD_TYPE)
else()
    set(BIN_DIRECTORY  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif(WIN32)

#include all directoried req for test execution
include_directories(${SOURCE_DIRECTORY})
include_directories(${TEST_DIRECTORY})
include_directories(${BIN_DIRECTORY})

## Get all test scripts
file(GLOB _FUNCTIONAL_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.java")

set(JAVA_LIB_PATH ${CMAKE_BINARY_DIR}/x64/release)
set(JAVA_DEBUG_FLAG "-Ddebug=0")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JAVA_LIB_PATH ${CMAKE_BINARY_DIR}/x64/debug)
    set(JAVA_DEBUG_FLAG "-Ddebug=1")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")
## Execute each test from the tests folder
foreach(_ftest_file ${_FUNCTIONAL_TEST_FILES})
  get_filename_component(_ftest_file_name "${_ftest_file}" NAME_WE)
  message(_ftest_file_name " ${_ftest_file_name} ")

  set(TEST_NAME test_java_${_ftest_file_name})
  set(TEST_FILE ${BIN_DIRECTORY}/${_ftest_file_name}) 
 
  add_custom_command(OUTPUT ${TEST_FILE}
     COMMAND ${Java_JAVAC_EXECUTABLE} -d ${BIN_DIRECTORY} ${SOURCE_DIRECTORY}/BScriptJNI.java ${TEST_DIRECTORY}/${_ftest_file_name}.java
     COMMENT "Creating ${TEST_FILE}")
 
  add_custom_target(${_ftest_file_name} ALL
    COMMAND echo "Java class files"
    DEPENDS ${TEST_FILE}
  )

  set_target_properties(${_ftest_file_name} PROPERTIES  FOLDER "test/java")
  add_test(NAME ${TEST_NAME} COMMAND ${Java_JAVA_EXECUTABLE} -Djava.library.path=${JAVA_LIB_PATH} ${JAVA_DEBUG_FLAG} -cp ${BIN_DIRECTORY}:${JUNIT_JAR}:${HAMCREST_JAR} org.junit.runner.JUnitCore ${JAVA_PACKAGE_NAME}.${_ftest_file_name})

endforeach()


