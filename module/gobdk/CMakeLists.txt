#################################################################
#  Date             08/11/2024                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2024 nChain Limited. All rights reserved       #
#################################################################


#### SESDK Go module version as increment of core version
createIncrementVersion(SESDK_GOLANG_VERSION_MAJOR ${SESDK_CORE_VERSION_MAJOR} 0 "script engine golang major version")
createIncrementVersion(SESDK_GOLANG_VERSION_MINOR ${SESDK_CORE_VERSION_MINOR} 1 "script engine golang minor version")
createIncrementVersion(SESDK_GOLANG_VERSION_PATCH ${SESDK_CORE_VERSION_PATCH} 0 "script engine golang patch version")
set(SESDK_GOLANG_VERSION_STRING "${SESDK_GOLANG_VERSION_MAJOR}.${SESDK_GOLANG_VERSION_MINOR}.${SESDK_GOLANG_VERSION_PATCH}")

## Test maximum of global version
if(${SESDK_VERSION_STRING} LESS ${SESDK_GOLANG_VERSION_STRING})
  message("Script Engine SDK global version should be the max of all version numbers")
  message("SESDK_VERSION_STRING=${SESDK_VERSION_STRING} while SESDK_GOLANG_VERSION_STRING=${SESDK_GOLANG_VERSION_STRING}")
endif()

#### Generate version golang binding #####
set(SESDK_VERSION_GOLANG_H_IN ${CMAKE_CURRENT_SOURCE_DIR}/cgo/include/version_go.h.in CACHE INTERNAL "Template File for python version")
set(SESDK_VERSION_GOLANG_H ${SESDK_GENERATED_HPP_DIR}/version_go.h CACHE INTERNAL "HPP File for python version")
configure_file(${SESDK_VERSION_GOLANG_H_IN} ${SESDK_VERSION_GOLANG_H})

set(_GO_MODULE_NAME GoSESDK)
set(_GO_MODULE_HEADER_FILES
    ${SESDK_VERSION_GOLANG_H}
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo/include/interpreter_cgo.h
)

set(_GO_MODULE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo/src/interpreter_cgo.cpp
)

add_library(${_GO_MODULE_NAME} SHARED ${_GO_MODULE_HEADER_FILES} ${_GO_MODULE_SOURCE_FILES})
target_link_libraries(${_GO_MODULE_NAME} PRIVATE sesdk_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 ${UNIVALUE_LIB} OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
target_include_directories(${_GO_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cgo/include)


add_custom_command(TARGET ${_GO_MODULE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${_GO_MODULE_NAME}>
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo/bin/$<TARGET_FILE_NAME:${_GO_MODULE_NAME}>
    COMMENT "Copying cgo library to the source"
)

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES FOLDER "module/golang" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_GO_MODULE_NAME} DESTINATION "bin" COMPONENT GoModules)
install(FILES ${SESDK_VERSION_GOLANG_H} ${_GO_MODULE_HEADER_FILES} DESTINATION "cgo/include" COMPONENT GoModules)
