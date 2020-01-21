#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
if(SCRYPTBuildSetting_Include)## Include guard
  return()
endif()
set(SCRYPTBuildSetting_Include TRUE)

macro(scryptSetCompilationOptions)## This has to be macro instead of a function because of add_definitions scope is local
  if(NOT SCRYPT_SET_COMPILATION_OPTIONS_DONE)
    ## Set common compile options
    set(CMAKE_CXX_STANDARD 17 CACHE INTERNAL "Use of C++17" FORCE)#TODO : change to C++17 when visual studio support
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE INTERNAL "Require C++ standard" FORCE)
    set(CMAKE_CXX_EXTENSIONS OFF CACHE INTERNAL "Do not use C++ extension" FORCE)
    set(CMAKE_DEBUG_POSTFIX d CACHE INTERNAL "Use d as debug postfix for  targets" FORCE)

    if (MSVC)
      scrypt_add_compiler_flag(/Gm- /MP /wd4251 /wd4244 /wd4307)## TODO test if DNOMINMAX can move here
      ## Using MSVC STATIC RUNTIME)
      ##foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      ##  if(${flag_var} MATCHES "/MD")
      ##    string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      ##  endif(${flag_var} MATCHES "/MD")
      ##endforeach()
    endif()

    if(UNIX)
      set(CMAKE_CXX_FLAGS "-fPIC -fvisibility=default" CACHE STRING "compilation flags for C++" FORCE)
      set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 " CACHE STRING "flags for C++ debug version" FORCE)
      set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -DNDEBUG" CACHE STRING "flags for C++ release version" FORCE)
      set(CMAKE_C_FLAGS "" CACHE STRING "flags for C" FORCE)
      set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "flags for C debug version" FORCE)
      set(CMAKE_C_FLAGS_RELEASE "-O3 -g0 -DNDEBUG" CACHE STRING "flags for C release version" FORCE)
    endif()

    set(SCRYPT_SET_COMPILATION_OPTIONS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()
endmacro()

#### Define output directories according to ${CMAKE_CONFIGURATION_TYPES} or ${CMAKE_BUILD_TYPE}
#### Based on the defined 
####   ${SCRYPT_SYSTEM_BUILD_ARCHI}
####   ${CMAKE_CONFIGURATION_TYPES}
####   ${CMAKE_BUILD_TYPE}
function(scryptSetOutputDirectories)

  if(SCRYPT_SET_OUTPUT_DIRS_DONE)
    return()
  else()
    set(SCRYPT_SET_OUTPUT_DIRS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()

  if(NOT DEFINED SCRYPT_SYSTEM_BUILD_ARCHI)
    message("  SetBuildFolders : variable [SCRYPT_SYSTEM_BUILD_ARCHI] is not defined")
    message(FATAL_ERROR "On Windows, [SCRYPT_SYSTEM_BUILD_ARCHI] should be defined x64 or x86")
  endif()

  if(CMAKE_BUILD_TYPE)
    set(_BuildTypeList "" "${CMAKE_BUILD_TYPE}")
  else()
    set(_BuildTypeList "${CMAKE_CONFIGURATION_TYPES}")
  endif()

  set(_LIST_OUTPUT_TYPES RUNTIME LIBRARY ARCHIVE)

  foreach(_BuildType IN LISTS _BuildTypeList)
    if(_BuildType)
      string(TOUPPER ${_BuildType} _BUILD_TYPE)
      string(TOLOWER ${_BuildType} _build_type_dir)
      set(_BUILD_TYPE_TAG "_${_BUILD_TYPE}")
    else()
      set(_BUILD_TYPE_TAG "")
      string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type_dir)
    endif()

    set(_MAIN_OUTPUT_PATH "${CMAKE_BINARY_DIR}/x${SCRYPT_SYSTEM_BUILD_ARCHI}/${_build_type_dir}")
    if(NOT EXISTS "${_MAIN_OUTPUT_PATH}")
      file(MAKE_DIRECTORY "${_MAIN_OUTPUT_PATH}")
    endif()
    foreach(_OUT_TYPE ${_LIST_OUTPUT_TYPES})
      set(CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG} "${_MAIN_OUTPUT_PATH}" CACHE PATH "Default path for runtime ouput directory" FORCE)
      scryptAppendToGlobalSet(scryptOUTPUT_DIRECTORIES "CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}=[${CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}}]")
    endforeach()
  endforeach()

  #scryptPrintList("scryptOUTPUT_DIRECTORIES" "${scryptOUTPUT_DIRECTORIES}")## Debug Log
endfunction()

function(scryptSetBuildVersion)
  if(SCRYPT_SET_BUILD_VERSION_DONE)
    return()
  else()
    set(SCRYPT_SET_BUILD_VERSION_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()

  if(NOT DEFINED SCRYPT_VERSION_MAJOR)
    set(SCRYPT_VERSION_MAJOR "1" CACHE INTERNAL "framework major version")
  endif()
  if(NOT DEFINED SCRYPT_VERSION_MINOR)
    set(SCRYPT_VERSION_MINOR "0" CACHE INTERNAL "framework minor version")
  endif()
  if(NOT DEFINED SCRYPT_VERSION_PATCH)
    set(SCRYPT_VERSION_PATCH "0" CACHE INTERNAL "framework patch version")
  endif()
  set(SCRYPT_VERSION_STRING "${SCRYPT_VERSION_MAJOR}.${SCRYPT_VERSION_MINOR}.${SCRYPT_VERSION_PATCH}" CACHE INTERNAL "scrypt version string")

  # Get branch
  scryptGetGitBranch(_SOURCE_GIT_COMMIT_BRANCH)
  set(SOURCE_GIT_COMMIT_BRANCH ${_SOURCE_GIT_COMMIT_BRANCH} CACHE INTERNAL "Source commit hash")

  scryptGetGitCommitHash(_SOURCE_GIT_COMMIT_HASH)
  set(SOURCE_GIT_COMMIT_HASH ${_SOURCE_GIT_COMMIT_HASH} CACHE INTERNAL "Source commit hash")

  # Get the latest commit datetime
  scryptGetGitCommitDateTime(_SOURCE_GIT_COMMIT_DATETIME)
  set(SOURCE_GIT_COMMIT_DATETIME ${_SOURCE_GIT_COMMIT_DATETIME} CACHE INTERNAL "Source commit datetime")

  # Get the build date time
  scryptGetBuildDateTime(_SCRYPT_BUILD_DATETIME_UTC)
  set(SCRYPT_BUILD_DATETIME_UTC ${_SCRYPT_BUILD_DATETIME_UTC} CACHE INTERNAL "Build datetime UTC")

  #### Generate version C++  #####
  set(SCRYPT_VERSION_HPP_IN ${SCRYPT_ROOT_CMAKE_MODULE_PATH}/SCRYPTVersion.hpp.in CACHE INTERNAL "Template File for framework version config")
  set(SCRYPT_VERSION_HPP ${SCRYPT_GENERATED_HPP_DIR}/SCRYPTVersion.hpp CACHE INTERNAL "HPP File for framework version config")
  configure_file(${SCRYPT_VERSION_HPP_IN} ${SCRYPT_VERSION_HPP})
endfunction()
