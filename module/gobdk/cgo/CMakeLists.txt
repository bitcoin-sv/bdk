#################################################################
#  Date             08/11/2024                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2024 nChain Limited. All rights reserved       #
#################################################################


#### BDK Go module version as increment of core version
createIncrementVersion(BDK_GOLANG_VERSION_MAJOR ${BDK_VERSION_MAJOR} 0 "BDK golang major version")
createIncrementVersion(BDK_GOLANG_VERSION_MINOR ${BDK_VERSION_MINOR} 0 "BDK golang minor version")
createIncrementVersion(BDK_GOLANG_VERSION_PATCH ${BDK_VERSION_PATCH} 2 "BDK golang patch version")
set(BDK_GOLANG_VERSION_STRING "${BDK_GOLANG_VERSION_MAJOR}.${BDK_GOLANG_VERSION_MINOR}.${BDK_GOLANG_VERSION_PATCH}")

## Test maximum of global version
if(${BDK_VERSION_STRING} LESS ${BDK_GOLANG_VERSION_STRING})
  message("Bitcoin Development Kit global version should be the max of all version numbers")
  message("BDK_VERSION_STRING=${BDK_VERSION_STRING} while BDK_GOLANG_VERSION_STRING=${BDK_GOLANG_VERSION_STRING}")
endif()

#### Generate version golang binding #####
set(BDK_VERSION_GOLANG_CPP_IN ${CMAKE_CURRENT_SOURCE_DIR}/src/version_cgo.cpp.in CACHE INTERNAL "Template File for golang version")
set(BDK_VERSION_GOLANG_CPP ${BDK_GENERATED_CPP_DIR}/cgo/version_cgo.cpp CACHE INTERNAL "Source File for golang version")
configure_file(${BDK_VERSION_GOLANG_CPP_IN} ${BDK_VERSION_GOLANG_CPP})

set(_GO_MODULE_NAME GoBDK)
set(_GO_MODULE_HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/asm_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/interpreter_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/gobdk.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/global_scriptconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/script_error_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cgo/version_cgo.h
)

set(_GO_MODULE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interpreter_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/global_scriptconfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/script_error_cgo.cpp
)

add_library(${_GO_MODULE_NAME} SHARED ${_GO_MODULE_HEADER_FILES} ${_GO_MODULE_SOURCE_FILES} ${BDK_VERSION_GOLANG_CPP})
target_link_libraries(${_GO_MODULE_NAME} PUBLIC bdk_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 ${UNIVALUE_LIB} OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
target_include_directories(${_GO_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
source_group("_generated" FILES "${BDK_VERSION_GOLANG_CPP}")

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES FOLDER "module/golang" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_GO_MODULE_NAME} DESTINATION "bin" COMPONENT GoModules)
install(FILES ${_GO_MODULE_HEADER_FILES} DESTINATION "include/cgo" COMPONENT GoModules)