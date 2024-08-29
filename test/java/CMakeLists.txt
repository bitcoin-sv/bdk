################################################################
#  Date             07/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindJavaJDKHelper)
HelpFindJavaJDK()
#scryptPrintJavaJDKInfo()#Debug Log

if(NOT (JNI_FOUND AND Java_FOUND AND BDK_JAR))
  message(STATUS "Unable to find Java JDK and script engine SDK")
  message(STATUS "       JNI_FOUND  [${JNI_FOUND}]")
  message(STATUS "       Java_FOUND [${Java_FOUND}]")
  message(STATUS "       BDK_JAR [${BDK_JAR}]")
  message(WARNING "Skip building Java tests")
  return()
endif()

if(DEFINED ENV{JAVA_TOOLS})
  set(JAVA_TOOLS_DIR "$ENV{JAVA_TOOLS}")
else()
  set(JAVA_TOOLS_DIR "${BDK_GENERATED_BIN_DIR}")
  message("\n  Dowloading testng and dependencies")
  message("    DOWNLOAD file https://repo1.maven.org/maven2/org/testng/testng/7.1.0/testng-7.1.0.jar      to ${JAVA_TOOLS_DIR}")
  file(DOWNLOAD https://repo1.maven.org/maven2/org/testng/testng/7.1.0/testng-7.1.0.jar "${JAVA_TOOLS_DIR}/testng-7.1.0.jar")
  message("    DOWNLOAD file https://repo1.maven.org/maven2/com/beust/jcommander/1.72/jcommander-1.72.jar to ${JAVA_TOOLS_DIR}")
  file(DOWNLOAD https://repo1.maven.org/maven2/com/beust/jcommander/1.72/jcommander-1.72.jar "${JAVA_TOOLS_DIR}/jcommander-1.72.jar")
  message("    DOWNLOAD file https://repo1.maven.org/maven2/com/google/inject/guice/4.1.0/guice-4.1.0.jar to ${JAVA_TOOLS_DIR}")
  file(DOWNLOAD https://repo1.maven.org/maven2/com/google/inject/guice/4.1.0/guice-4.1.0.jar "${JAVA_TOOLS_DIR}/guice-4.1.0.jar")
  message("    DOWNLOAD file https://repo1.maven.org/maven2/org/yaml/snakeyaml/1.21/snakeyaml-1.21.jar    to ${JAVA_TOOLS_DIR}")
  file(DOWNLOAD https://repo1.maven.org/maven2/org/yaml/snakeyaml/1.21/snakeyaml-1.21.jar "${JAVA_TOOLS_DIR}/snakeyaml-1.21.jar")
  message("  Skip these download by manually downloading them to a directory.")
  message("  Then define the environment variable JAVA_TOOLS pointing to this directory\n")
endif()
message(STATUS "Use JAVA_TOOLS_DIR=[${JAVA_TOOLS_DIR}]")

find_jar(TESTNG_JAR NAMES testng-7.1.0 PATHS ${JAVA_TOOLS_DIR} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})
find_jar(JCOMMANDER_JAR NAMES jcommander-1.72 PATHS ${JAVA_TOOLS_DIR} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})
find_jar(GUICE_JAR NAMES guice-4.1.0 PATHS ${JAVA_TOOLS_DIR} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})
find_jar(SNAKEYAML_JAR NAMES snakeyaml-1.21 PATHS ${JAVA_TOOLS_DIR} $ENV{JAVA_HOME} $ENV{JAVA_ROOT})

if(NOT (TESTNG_JAR AND JCOMMANDER_JAR AND GUICE_JAR AND SNAKEYAML_JAR))
  message(STATUS "Unable to find Java testing packages for testng")
  message(STATUS "       TESTNG_JAR     [${TESTNG_JAR}]")
  message(STATUS "       JCOMMANDER_JAR [${JCOMMANDER_JAR}]")
  message(STATUS "       GUICE_JAR      [${GUICE_JAR}]")
  message(STATUS "       SNAKEYAML_JAR  [${SNAKEYAML_JAR}]")
  message(WARNING "Skip building Java tests")
  return()
endif()

set(BDK_CLASSPATH "com.nchain.bdk")

## Get all test scripts
file(GLOB _JAVA_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*Test.java")


if(CMAKE_CONFIGURATION_TYPES)
  set(TESTNG_JAR_FILES "${TESTNG_JAR}\$<SEMICOLON>${JCOMMANDER_JAR}\$<SEMICOLON>${GUICE_JAR}\$<SEMICOLON>${SNAKEYAML_JAR}")
else()
  set(TESTNG_JAR_FILES "${TESTNG_JAR}:${JCOMMANDER_JAR}:${GUICE_JAR}:${SNAKEYAML_JAR}")
endif()

## Execute each test from the tests folder
foreach(java_testfile ${_JAVA_TEST_FILES})
  get_filename_component(JAVA_TEST_CLASSNAME "${java_testfile}" NAME_WE)
  string(REPLACE "Test" "" BASE_TEST_NAME ${JAVA_TEST_CLASSNAME})

  set(JAVA_BUILDTEST_NAME build_Java${JAVA_TEST_CLASSNAME})
  set(JAVA_RUNTEST_NAME test_Java${BASE_TEST_NAME})

  add_custom_target(${JAVA_BUILDTEST_NAME} ALL DEPENDS "${java_testfile}")
  add_dependencies(${JAVA_BUILDTEST_NAME} ${BDK_JAR_BASE_NAME})

  # Compile java test to runtime output directories of the project
  # -cp should have all *.jar files, in particular the ${BDK_JAR}
  if(CMAKE_CONFIGURATION_TYPES)
    add_custom_command(TARGET ${JAVA_BUILDTEST_NAME}
      COMMAND ${Java_JAVAC_EXECUTABLE}
      -d $<IF:$<CONFIG:Debug>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG},${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>
      -cp "${BDK_JAR}\$<SEMICOLON>${TESTNG_JAR_FILES}"
      ${java_testfile}
       COMMENT "Compiling ${java_testfile}")
  else()
    add_custom_command(TARGET ${JAVA_BUILDTEST_NAME}
      COMMAND ${Java_JAVAC_EXECUTABLE}
      -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -cp "${BDK_JAR}:${TESTNG_JAR_FILES}"
      ${java_testfile}
       COMMENT "Compiling ${java_testfile}")
  endif()

  set_target_properties(${JAVA_BUILDTEST_NAME} PROPERTIES  FOLDER "test/java")

  # Add java test
  # -Djava.library.path should point to the jni runtime library (C++)
  # -cp should have all *.jar files, in particular the ${BDK_JAR}
  if(CMAKE_CONFIGURATION_TYPES)
    add_test(NAME ${JAVA_RUNTEST_NAME}${CMAKE_DEBUG_POSTFIX}
      CONFIGURATIONS Debug
      COMMAND ${Java_JAVA_EXECUTABLE} -Ddebug=1
      -Djava_test_data_dir=${CMAKE_CURRENT_SOURCE_DIR}/data
      -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
      -cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}\$<SEMICOLON>${BDK_JAR}\$<SEMICOLON>${TESTNG_JAR_FILES}" org.testng.TestNG
      -testclass "${BDK_CLASSPATH}.${JAVA_TEST_CLASSNAME}"
      -suitename "."
      -testname ${JAVA_RUNTEST_NAME}${CMAKE_DEBUG_POSTFIX}
      -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
      )
    add_test(NAME ${JAVA_RUNTEST_NAME}
      CONFIGURATIONS Release
      COMMAND ${Java_JAVA_EXECUTABLE} -Ddebug=0
      -Djava_test_data_dir=${CMAKE_CURRENT_SOURCE_DIR}/data
      -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
      -cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}\$<SEMICOLON>${BDK_JAR}\$<SEMICOLON>${TESTNG_JAR_FILES}" org.testng.TestNG
      -testclass ${BDK_CLASSPATH}.${JAVA_TEST_CLASSNAME}
      -suitename "."
      -testname ${JAVA_RUNTEST_NAME}
      -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
      )
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
      set(_tdflag "-Ddebug=1")
    else()
      set(_tdflag "-Ddebug=0")
    endif()
    add_test(NAME ${JAVA_RUNTEST_NAME}${_tpost_fix}
      COMMAND ${Java_JAVA_EXECUTABLE} ${_tdflag}
      -Djava_test_data_dir=${CMAKE_CURRENT_SOURCE_DIR}/data
      -Djava.library.path=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}:${BDK_JAR}:${JAVA_TOOLS_DIR}:${TESTNG_JAR_FILES}" org.testng.TestNG
      -testclass ${BDK_CLASSPATH}.${JAVA_TEST_CLASSNAME}
      -suitename "."
      -testname ${JAVA_RUNTEST_NAME}${_tpost_fix}
      -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
  endif()
endforeach()
