################################################################
#  Date             07/05/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindJavaJDKHelper)
HelpFindJavaJDK()
#bdkPrintJavaJDKInfo()#Debug Log

if(NOT (JNI_FOUND AND Java_FOUND))
  message(WARNING "Unable to find Java JDK on the system. Skip building java binding module")
  return()
endif()
set(BDK_JAR_BASE_NAME bdk CACHE INTERNAL "Name of the Bitcoin Development Kit Java package")
set(BDK_JAVA_JNI_NAME JavaBDK CACHE INTERNAL "Name of the Bitcoin Development Kit JNI runtime library")

#### Java module version as increment of core version
##createIncrementVersion(BDK_JAVA_VERSION_MAJOR ${BDK_CORE_VERSION_MAJOR} 1 "BDK java major version")
##createIncrementVersion(BDK_JAVA_VERSION_MINOR ${BDK_CORE_VERSION_MINOR} 0 "BDK java minor version")
##createIncrementVersion(BDK_JAVA_VERSION_PATCH ${BDK_CORE_VERSION_PATCH} 0 "BDK java patch version")
set(BDK_JAVA_VERSION_MAJOR 1)
set(BDK_JAVA_VERSION_MINOR 0)
set(BDK_JAVA_VERSION_PATCH 1)
set(BDK_JAVA_VERSION_STRING "${BDK_JAVA_VERSION_MAJOR}.${BDK_JAVA_VERSION_MINOR}.${BDK_JAVA_VERSION_PATCH}")

## Test maximum of global version
if(${BDK_VERSION_STRING} LESS ${BDK_JAVA_VERSION_STRING})
  message("Bitcoin Development Kit global version should be the max of all version numbers")
  message("BDK_VERSION_STRING=${BDK_VERSION_STRING} while BDK_JAVA_VERSION_STRING=${BDK_JAVA_VERSION_STRING}")
endif()

#### Generate config file for java  #####
set(PACKAGE_INFO_JAVA_IN ${CMAKE_CURRENT_SOURCE_DIR}/PackageInfo.java.in CACHE INTERNAL "Template package info for java BDK")
set(PACKAGE_INFO_JAVA ${BDK_GENERATED_TOOLS_DIR}/PackageInfo.java CACHE INTERNAL "Package info for java BDK")
configure_file(${PACKAGE_INFO_JAVA_IN} ${PACKAGE_INFO_JAVA})

#####  Building jar package for Bitcoin Development Kit
file(GLOB JAVA_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.java)
set(JAVA_SRC_FILES ${JAVA_SRC_FILES} ${PACKAGE_INFO_JAVA})

set(BDK_JAR_VER_NAME ${BDK_JAR_BASE_NAME}-${BDK_JAVA_VERSION_STRING})
add_jar(${BDK_JAR_BASE_NAME}
  SOURCES ${JAVA_SRC_FILES}
  GENERATE_NATIVE_HEADERS jni_interface DESTINATION "${BDK_GENERATED_HPP_DIR}"
  OUTPUT_NAME ${BDK_JAR_VER_NAME}
  OUTPUT_DIR "${BDK_GENERATED_BIN_DIR}"
)

set(BDK_JAR "${BDK_GENERATED_BIN_DIR}/${BDK_JAR_VER_NAME}.jar" CACHE INTERNAL "Absolute path to bdk jar file")# BDK_JAR use to run java tests
set_target_properties(${BDK_JAR_BASE_NAME} PROPERTIES FOLDER "module/java")
install_jar(${BDK_JAR_BASE_NAME} DESTINATION "bin" COMPONENT JavaModule)

#####  Building jni runtime native librarie
set(BDK_JNI_SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_cppobj_helper.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_memory.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_assembler.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_cancellation_token.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_script_iterator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_scriptengine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/jni_stack.cpp"
)
add_library(${BDK_JAVA_JNI_NAME} SHARED ${BDK_JNI_SRC_FILES})

if(WIN32) #https://github.com/openssl/openssl/pull/1062
  target_link_libraries(${BDK_JAVA_JNI_NAME} crypt32 ws2_32)
  set_target_properties(${BDK_JAVA_JNI_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()
target_include_directories(${BDK_JAVA_JNI_NAME} PUBLIC ${JNI_INCLUDE_DIRS} ${BDK_GENERATED_HPP_DIR})
target_link_libraries(${BDK_JAVA_JNI_NAME} bdk_core jni_interface ${JNI_LIBRARIES})

set_target_properties(${BDK_JAVA_JNI_NAME} PROPERTIES FOLDER "module/java" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
install(TARGETS ${BDK_JAVA_JNI_NAME} DESTINATION "bin" COMPONENT JavaModule)
