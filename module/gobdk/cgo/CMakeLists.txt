#################################################################
#  Date             08/11/2024                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2024 nChain Limited. All rights reserved       #
#################################################################


#### BDK Go module version as increment of core version
createIncrementVersion(BDK_GOLANG_VERSION_MAJOR ${BDK_VERSION_MAJOR} 0 "BDK golang major version")
createIncrementVersion(BDK_GOLANG_VERSION_MINOR ${BDK_VERSION_MINOR} 1 "BDK golang minor version")
createIncrementVersion(BDK_GOLANG_VERSION_PATCH ${BDK_VERSION_PATCH} 0 "BDK golang patch version")
set(BDK_GOLANG_VERSION_STRING "${BDK_GOLANG_VERSION_MAJOR}.${BDK_GOLANG_VERSION_MINOR}.${BDK_GOLANG_VERSION_PATCH}")

## Test maximum of global version
if(${BDK_VERSION_STRING} LESS ${BDK_GOLANG_VERSION_STRING})
  message("Bitcoin Development Kit global version should be the max of all version numbers")
  message("BDK_VERSION_STRING=${BDK_VERSION_STRING} while BDK_GOLANG_VERSION_STRING=${BDK_GOLANG_VERSION_STRING}")
endif()

#### Generate version golang binding #####
set(BDK_HEADER_GLOB_IN ${CMAKE_CURRENT_SOURCE_DIR}/include/gobdk.h.in CACHE INTERNAL "File globing all cgo header files in installation")
set(BDK_VERSION_GOLANG_H_IN ${CMAKE_CURRENT_SOURCE_DIR}/include/version_cgo.h.in CACHE INTERNAL "Template File for golang version")
set(BDK_VERSION_GOLANG_H ${BDK_GENERATED_HPP_DIR}/version_cgo.h CACHE INTERNAL "Header File for golang version")
configure_file(${BDK_VERSION_GOLANG_H_IN} ${BDK_VERSION_GOLANG_H})

set(_GO_MODULE_NAME GoBDK)
set(_GO_MODULE_HEADER_FILES
    ${BDK_VERSION_GOLANG_H}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/asm_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/interpreter_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/global_scriptconfig.h
)

set(_GO_MODULE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interpreter_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/global_scriptconfig.cpp
)

add_library(${_GO_MODULE_NAME} SHARED ${_GO_MODULE_HEADER_FILES} ${_GO_MODULE_SOURCE_FILES})
target_link_libraries(${_GO_MODULE_NAME} PRIVATE bdk_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 ${UNIVALUE_LIB} OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
target_include_directories(${_GO_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES FOLDER "module/golang" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_GO_MODULE_NAME} DESTINATION "bin" COMPONENT GoModules)
install(FILES ${BDK_VERSION_GOLANG_H} ${_GO_MODULE_HEADER_FILES} DESTINATION "include/cgo" COMPONENT GoModules)
install(FILES ${BDK_HEADER_GLOB_IN} DESTINATION "include/cgo" RENAME "gobdk.h" COMPONENT GoModules)