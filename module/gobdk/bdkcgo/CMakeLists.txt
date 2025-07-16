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
set(BDK_VERSION_GOLANG_CPP ${BDK_GENERATED_CPP_DIR}/bdkcgo/src/version_cgo.cpp CACHE INTERNAL "Source File for golang version")
configure_file(${BDK_VERSION_GOLANG_CPP_IN} ${BDK_VERSION_GOLANG_CPP})

set(_GO_MODULE_NAME GoBDK)
set(_GO_MODULE_HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/asm_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gobdk.h
    ${CMAKE_CURRENT_SOURCE_DIR}/script_error_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptengine_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/version_cgo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1_cloned.h # added here to be installed but won't be include twice as it use the same guard SECP256K1_H
)

set(_GO_MODULE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/script_error_cgo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptengine_cgo.cpp
)

# The shared libraries is not to be used in a normal situation. But we keep building this target
# just in case we might use it sometime later.
add_library(${_GO_MODULE_NAME} SHARED ${_GO_MODULE_HEADER_FILES} ${_GO_MODULE_SOURCE_FILES} ${BDK_VERSION_GOLANG_CPP})
target_link_libraries(${_GO_MODULE_NAME} PUBLIC bdk_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
target_include_directories(${_GO_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR})

# The static libraries is built and to be merge will all of its dependant archives
# to make a all-in-one standalone static library, ready to be linked in cgo
add_library(static-${_GO_MODULE_NAME} STATIC ${_GO_MODULE_HEADER_FILES} ${_GO_MODULE_SOURCE_FILES} ${BDK_VERSION_GOLANG_CPP})
target_link_libraries(static-${_GO_MODULE_NAME} PUBLIC bdk_core ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
target_include_directories(static-${_GO_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR})

source_group("_generated" FILES "${BDK_VERSION_GOLANG_CPP}")

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES FOLDER "module/golang" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(static-${_GO_MODULE_NAME} PROPERTIES FOLDER "module/golang" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(${_GO_MODULE_NAME} PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()

install(TARGETS ${_GO_MODULE_NAME} DESTINATION "bin/bdkcgo" COMPONENT GoModules)
install(TARGETS static-${_GO_MODULE_NAME} DESTINATION "lib" COMPONENT GoModules)
install(FILES ${_GO_MODULE_HEADER_FILES} DESTINATION "include/bdkcgo" COMPONENT GoModules)

## Merge the gobdk only for unix base system
if(UNIX)
  set(listArchive)
  # Get list of archive from 3rd party libraries ##########################
  foreach(impTarget IN ITEMS OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
    get_property(vImported TARGET ${impTarget} PROPERTY IMPORTED)
    if (${vImported})
      get_property(vLocation TARGET ${impTarget} PROPERTY LOCATION)
      list(APPEND listArchive ${vLocation})
    else()
      message(FATAL_ERROR "${impTarget} is not an imported target")
    endif()
  endforeach()
  # Get list of archive from our build ####################################
  set(buildtype "release")
  set(debugPostfix "")
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(buildtype "debug")
    set(debugPostfix "d")
  endif()
  set(buildDir ${CMAKE_BINARY_DIR}/x64/${buildtype})
  foreach(buildTarget IN ITEMS univalue secp256k1 bdk_core static-${_GO_MODULE_NAME})
    set(buildFile "${buildDir}/lib${buildTarget}${debugPostfix}.a")
    list(APPEND listArchive ${buildFile})
  endforeach()

  # extract all archive files to object files #############################
  foreach(archive IN LISTS listArchive)
    get_filename_component(archiveName ${archive} NAME_WE)
    set(objDir "${buildDir}/merge_gobdk/${archiveName}")
    file(MAKE_DIRECTORY "${objDir}")

    add_custom_command(TARGET static-${_GO_MODULE_NAME} POST_BUILD
    COMMAND ${CMAKE_AR} x ${archive}
    WORKING_DIRECTORY "${objDir}"
    COMMENT "Extract object files from [${archive}]")
  endforeach()

  # merge all object files to a single archive file #######################
  # OsArch variable is used to define the static libraries used in CGo build
  # By default, it reads from the system by $CMAKE_SYSTEM_NAME and $CMAKE_SYSTEM_PROCESSOR
  # Though in special case (e.g macos arm but forced to build x86_64), user might need to
  # be able to customize this os_arch. In that case, add the -DCUSTOM_GOBDK_OS_ARCH= to define it.
  # For example, on macos-15, add to cmake command -DCUSTOM_GOBDK_OS_ARCH=darwin_x86_64 to build
  # x86_64 file
  if(DEFINED CUSTOM_GOBDK_OS_ARCH)
    set(OsArch "${CUSTOM_GOBDK_OS_ARCH}")
    message("OsArch=${CUSTOM_GOBDK_OS_ARCH} was used by custom value")
  else()
    set(OsArch "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
    message("OsArch=${CUSTOM_GOBDK_OS_ARCH} used by default values read from the system")
  endif()
  string(TOLOWER ${OsArch} os_arch)
  set(mergedArchive "libGoBDK_${os_arch}.a")
  add_custom_target(MergeGoBDK ALL
                  COMMAND ${CMAKE_AR} rcs "${mergedArchive}" "${buildDir}/merge_gobdk/*/*.o"
                  DEPENDS static-${_GO_MODULE_NAME}
                  WORKING_DIRECTORY ${buildDir}
                  COMMENT "Merge all in one ${mergedArchive}")

  install(FILES "${buildDir}/${mergedArchive}" DESTINATION "lib/bdkcgo" COMPONENT GoModules)
  if (BUILD_MODULE_GOLANG_INSTALL_INSOURCE)
    add_custom_command(TARGET MergeGoBDK POST_BUILD COMMAND cp "${buildDir}/${mergedArchive}" "${CMAKE_SOURCE_DIR}/module/gobdk/bdkcgo/${mergedArchive}"
      COMMENT "Install insource ${mergedArchive}"
    )
  endif()

endif()