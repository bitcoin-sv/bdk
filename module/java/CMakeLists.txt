################################################################
#  Date             07/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindJavaJDKHelper)
HelpFindJavaJDK()
#scryptPrintJavaJDKInfo()#Debug Log

if(NOT (JNI_FOUND AND Java_FOUND))
  message(WARNING "Unable to find Java JDK on the system. Skip building java binding module")
  return()
endif()
set(SESDK_JAR_BASE_NAME sesdk CACHE INTERNAL "Name of the Script Engine SDK Java package")
set(SESDK_JAVA_JNI_NAME sesdk_jni CACHE INTERNAL "Name of the Script Engine SDK JNI runtime library")

#### Java module version as increment of core version
##createIncrementVersion(SESDK_JAVA_VERSION_MAJOR ${SESDK_CORE_VERSION_MAJOR} 1 "script engine java major version")
##createIncrementVersion(SESDK_JAVA_VERSION_MINOR ${SESDK_CORE_VERSION_MINOR} 0 "script engine java minor version")
##createIncrementVersion(SESDK_JAVA_VERSION_PATCH ${SESDK_CORE_VERSION_PATCH} 0 "script engine java patch version")
set(SESDK_JAVA_VERSION_MAJOR 1)
set(SESDK_JAVA_VERSION_MINOR 0)
set(SESDK_JAVA_VERSION_PATCH 1)
set(SESDK_JAVA_VERSION_STRING "${SESDK_JAVA_VERSION_MAJOR}.${SESDK_JAVA_VERSION_MINOR}.${SESDK_JAVA_VERSION_PATCH}")

## Test maximum of global version
if(${SESDK_VERSION_STRING} LESS ${SESDK_JAVA_VERSION_STRING})
  message("Script Engine SDK global version should be the max of all version numbers")
  message("SESDK_VERSION_STRING=${SESDK_VERSION_STRING} while SESDK_JAVA_VERSION_STRING=${SESDK_JAVA_VERSION_STRING}")
endif()

#### Generate config file for java  #####
set(PACKAGE_INFO_JAVA_IN ${CMAKE_CURRENT_SOURCE_DIR}/PackageInfo.java.in CACHE INTERNAL "Template package info for java script engine")
set(PACKAGE_INFO_JAVA ${SESDK_GENERATED_TOOLS_DIR}/PackageInfo.java CACHE INTERNAL "Package info for java script engine")
configure_file(${PACKAGE_INFO_JAVA_IN} ${PACKAGE_INFO_JAVA})

#####  Building jar package for Script Engine SDK
file(GLOB JAVA_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.java)
set(JAVA_SRC_FILES ${JAVA_SRC_FILES} ${PACKAGE_INFO_JAVA})

set(SESDK_JAR_VER_NAME ${SESDK_JAR_BASE_NAME}-${SESDK_JAVA_VERSION_STRING})
add_jar(${SESDK_JAR_BASE_NAME}
  SOURCES ${JAVA_SRC_FILES}
  GENERATE_NATIVE_HEADERS jni_interface DESTINATION "${SESDK_GENERATED_HPP_DIR}"
  OUTPUT_NAME ${SESDK_JAR_VER_NAME}
  OUTPUT_DIR "${SESDK_GENERATED_BIN_DIR}"
)

set(SESDK_JAR "${SESDK_GENERATED_BIN_DIR}/${SESDK_JAR_VER_NAME}.jar" CACHE INTERNAL "Absolute path to sesdk jar file")# SESDK_JAR use to run java tests
set_target_properties(${SESDK_JAR_BASE_NAME} PROPERTIES FOLDER "module/java")
install_jar(${SESDK_JAR_BASE_NAME} DESTINATION "lib" COMPONENT module)

#####  Building jni runtime native librarie
set(SESDK_JNI_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_cppobj_helper.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_memory.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_assembler.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_cancellation_token.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_script_iterator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_scriptengine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_stack.cpp"
)
add_library(${SESDK_JAVA_JNI_NAME} SHARED ${SESDK_JNI_SRC_FILES})

if(WIN32) #https://github.com/openssl/openssl/pull/1062
  target_link_libraries(${SESDK_JAVA_JNI_NAME} crypt32 ws2_32)
  set_target_properties(${SESDK_JAVA_JNI_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()
target_include_directories(${SESDK_JAVA_JNI_NAME} PUBLIC ${JNI_INCLUDE_DIRS} ${SESDK_GENERATED_HPP_DIR})
target_link_libraries(${SESDK_JAVA_JNI_NAME} sesdk_core jni_interface ${JNI_LIBRARIES})

set_target_properties(${SESDK_JAVA_JNI_NAME} PROPERTIES FOLDER "module/java" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
install(TARGETS ${SESDK_JAVA_JNI_NAME} DESTINATION "lib" COMPONENT module)
