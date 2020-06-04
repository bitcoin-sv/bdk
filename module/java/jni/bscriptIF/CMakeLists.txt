cmake_minimum_required(VERSION 3.12)
project(JNIScriptIF)
find_package(JNI REQUIRED)
find_package(OpenSSL REQUIRED)
FIND_PACKAGE(Java REQUIRED)

INCLUDE(UseJava)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_jar(BScriptJNI SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/BScriptJNI.java GENERATE_NATIVE_HEADERS BSVUtilsJNI-native DESTINATION ${SCRYPT_GENERATED_HPP_DIR} OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
    else()
        add_jar(BScriptJNI SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/BScriptJNI.java GENERATE_NATIVE_HEADERS BSVUtilsJNI-native DESTINATION ${SCRYPT_GENERATED_HPP_DIR} OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
    endif(CMAKE_BUILD_TYPE STREQUAL "Debug")
else()
add_jar(BScriptJNI ${CMAKE_CURRENT_SOURCE_DIR}/BScriptJNI.java GENERATE_NATIVE_HEADERS BSVUtilsJNI-native DESTINATION ${SCRYPT_GENERATED_HPP_DIR} OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif(WIN32)

set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/jni_bscriptIF_BScriptJNI.cpp)

add_library(jniBSCript SHARED ${SOURCE_FILES})
if(WIN32) #https://github.com/openssl/openssl/pull/1062
  target_link_libraries(jniBSCript crypt32 ws2_32)
  set_target_properties(jniBSCript PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()
target_include_directories(jniBSCript PUBLIC ${JNI_INCLUDE_DIRS} ${JAVA_INCLUDE_PATH2})
target_link_libraries(jniBSCript ScriptEngineIF OpenSSL::Crypto OpenSSL::SSL BSVUtilsJNI-native)
set_target_properties(jniBSCript PROPERTIES  FOLDER "module/java" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

install(TARGETS jniBSCript DESTINATION "lib" COMPONENT module)
install_jar(BScriptJNI DESTINATION "lib" COMPONENT module)

