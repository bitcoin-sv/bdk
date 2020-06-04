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

## Get all test scripts
file(GLOB _FUNCTIONAL_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.java")

if(CMAKE_CONFIGURATION_TYPES)
  set(JAVA_DEBUG_FLAG "\$<IF:\$<CONFIG:Debug>,-Ddebug=1,-Ddebug=0>")
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JAVA_DEBUG_FLAG "-Ddebug=1")
  else()
    set(JAVA_DEBUG_FLAG "-Ddebug=0")
  endif()
endif()

## Execute each test from the tests folder
foreach(_ftest_file ${_FUNCTIONAL_TEST_FILES})
  get_filename_component(_ftest_file_name "${_ftest_file}" NAME_WE)
  message(_ftest_file_name " ${_ftest_file_name} ")

  set(TEST_NAME test_java_${_ftest_file_name})

  add_custom_target(${_ftest_file_name} ALL DEPENDS ${TEST_DIRECTORY}/${_ftest_file_name}.java)

  if(CMAKE_CONFIGURATION_TYPES)
    add_custom_command(TARGET ${_ftest_file_name}
       COMMAND ${Java_JAVAC_EXECUTABLE} -d $<IF:$<CONFIG:Debug>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG},${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}> ${SOURCE_DIRECTORY}/BScriptJNI.java ${TEST_DIRECTORY}/${_ftest_file_name}.java
       COMMENT "Compiling ${TEST_FILE}")
  else()
    add_custom_command(TARGET ${_ftest_file_name}
       COMMAND ${Java_JAVAC_EXECUTABLE} -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${SOURCE_DIRECTORY}/BScriptJNI.java ${TEST_DIRECTORY}/${_ftest_file_name}.java
       COMMENT "Compiling ${TEST_FILE}")
  endif()

  set_target_properties(${_ftest_file_name} PROPERTIES  FOLDER "test/java")

  if(CMAKE_CONFIGURATION_TYPES)
    ## Need to set the WORKING_DIRECTORY on windows as a hack to get the PATH set there
    ## -Djava.library.path alone does not work because the jni dll depend to other dll in this directory (chained dependencies)
    add_test(NAME ${TEST_NAME}d
             CONFIGURATIONS Debug
             COMMAND ${Java_JAVA_EXECUTABLE} -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG} ${JAVA_DEBUG_FLAG} -cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}\$<SEMICOLON>${JUNIT_JAR}\$<SEMICOLON>${HAMCREST_JAR}" org.junit.runner.JUnitCore ${JAVA_PACKAGE_NAME}.${_ftest_file_name}
             WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
    )
    add_test(NAME ${TEST_NAME}
             CONFIGURATIONS Release
             COMMAND ${Java_JAVA_EXECUTABLE} -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} ${JAVA_DEBUG_FLAG} -cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}\$<SEMICOLON>${JUNIT_JAR}\$<SEMICOLON>${HAMCREST_JAR}" org.junit.runner.JUnitCore ${JAVA_PACKAGE_NAME}.${_ftest_file_name}
             WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
    )
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
    endif()
    add_test(NAME ${TEST_NAME}${_tpost_fix}
             COMMAND ${Java_JAVA_EXECUTABLE} -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${JAVA_DEBUG_FLAG} -cp ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}:${JUNIT_JAR}:${HAMCREST_JAR} org.junit.runner.JUnitCore ${JAVA_PACKAGE_NAME}.${_ftest_file_name}
    )
  endif()
endforeach()


