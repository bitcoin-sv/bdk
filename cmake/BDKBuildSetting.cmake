#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
if(BDKBuildSetting_Include)## Include guard
  return()
endif()
set(BDKBuildSetting_Include TRUE)

macro(bdkSetCompilationOptions)## This has to be macro instead of a function because of add_definitions scope is local
  if(NOT BDK_SET_COMPILATION_OPTIONS_DONE)
    ## Set common compile options
    set(CMAKE_CXX_STANDARD 20 CACHE INTERNAL "Use of C++20" FORCE)#TODO : change to C++20 when visual studio support
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE INTERNAL "Require C++ standard" FORCE)
    set(CMAKE_CXX_EXTENSIONS OFF CACHE INTERNAL "Do not use C++ extension" FORCE)
    set(CMAKE_DEBUG_POSTFIX d CACHE INTERNAL "Use d as debug postfix for  targets" FORCE)

    if (MSVC)
      bdk_add_compiler_flag(/Gm- /MP /wd4251 /wd4244 /wd4307)## TODO test if DNOMINMAX can move here
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
      set(CMAKE_C_FLAGS "-fPIC" CACHE STRING "flags for C" FORCE)
      set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "flags for C debug version" FORCE)
      set(CMAKE_C_FLAGS_RELEASE "-O3 -g0 -DNDEBUG" CACHE STRING "flags for C release version" FORCE)
    endif()

    set(BDK_SET_COMPILATION_OPTIONS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()
endmacro()

#### Define output directories according to ${CMAKE_CONFIGURATION_TYPES} or ${CMAKE_BUILD_TYPE}
#### Based on the defined 
####   ${BDK_SYSTEM_BUILD_ARCHI}
####   ${CMAKE_CONFIGURATION_TYPES}
####   ${CMAKE_BUILD_TYPE}
function(bdkSetOutputDirectories)

  if(BDK_SET_OUTPUT_DIRS_DONE)
    return()
  else()
    set(BDK_SET_OUTPUT_DIRS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()

  if(NOT DEFINED BDK_SYSTEM_BUILD_ARCHI)
    message("  SetBuildFolders : variable [BDK_SYSTEM_BUILD_ARCHI] is not defined")
    message(FATAL_ERROR "On Windows, [BDK_SYSTEM_BUILD_ARCHI] should be defined x64 or x86")
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

    set(_MAIN_OUTPUT_PATH "${CMAKE_BINARY_DIR}/x${BDK_SYSTEM_BUILD_ARCHI}/${_build_type_dir}")
    if(NOT EXISTS "${_MAIN_OUTPUT_PATH}")
      file(MAKE_DIRECTORY "${_MAIN_OUTPUT_PATH}")
    endif()
    foreach(_OUT_TYPE ${_LIST_OUTPUT_TYPES})
      set(CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG} "${_MAIN_OUTPUT_PATH}" CACHE PATH "Default path for runtime ouput directory" FORCE)
      bdkAppendToGlobalSet(bdkOUTPUT_DIRECTORIES "CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}=[${CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}}]")
    endforeach()
  endforeach()

  #bdkPrintList("bdkOUTPUT_DIRECTORIES" "${bdkOUTPUT_DIRECTORIES}")## Debug Log
endfunction()

#### Get bsv versions in the "${BDK_BSV_ROOT_DIR}/src/clientversion.h"
function(bdkCalculateBSVVersion clientversionFile)###############################################################
  if(NOT EXISTS "${clientversionFile}")
    message(FATAL_ERROR "BSV version file [${clientversionFile}] does not exist")
  endif()

  file(STRINGS ${clientversionFile} _CLIENT_VERSION_MAJOR_TMP REGEX "^#define CLIENT_VERSION_MAJOR[ \t]+[0-9]+[ \t]*$")
  file(STRINGS ${clientversionFile} _CLIENT_VERSION_MINOR_TMP REGEX "^#define CLIENT_VERSION_MINOR[ \t]+[0-9]+[ \t]*$")
  file(STRINGS ${clientversionFile} _CLIENT_VERSION_REVISION_TMP REGEX "^#define CLIENT_VERSION_REVISION[ \t]+[0-9]+[ \t]*$")

  string(REPLACE "#define CLIENT_VERSION_MAJOR " "" _CLIENT_VERSION_MAJOR ${_CLIENT_VERSION_MAJOR_TMP})
  string(REPLACE "#define CLIENT_VERSION_MINOR " "" _CLIENT_VERSION_MINOR ${_CLIENT_VERSION_MINOR_TMP})
  string(REPLACE "#define CLIENT_VERSION_REVISION " "" _CLIENT_VERSION_REVISION ${_CLIENT_VERSION_REVISION_TMP})

  set(BSV_CLIENT_VERSION_MAJOR ${_CLIENT_VERSION_MAJOR} CACHE INTERNAL "BSV CLIENT_VERSION_MAJOR")
  set(BSV_CLIENT_VERSION_MINOR ${_CLIENT_VERSION_MINOR} CACHE INTERNAL "BSV CLIENT_VERSION_MINOR")
  set(BSV_CLIENT_VERSION_REVISION ${_CLIENT_VERSION_REVISION} CACHE INTERNAL "BSV CLIENT_VERSION_REVISION")

  set(BSV_VERSION_STRING "${BSV_CLIENT_VERSION_MAJOR}.${BSV_CLIENT_VERSION_MINOR}.${BSV_CLIENT_VERSION_REVISION}" CACHE INTERNAL "BSV version string")
endfunction()

function(bdkSetBuildVersion)
  if(BDK_SET_BUILD_VERSION_DONE)
    return()
  else()
    set(BDK_SET_BUILD_VERSION_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()

  if(NOT DEFINED BDK_VERSION_MAJOR)
    set(BDK_VERSION_MAJOR "1" CACHE INTERNAL "framework major version")
  endif()
  if(NOT DEFINED BDK_VERSION_MINOR)
    set(BDK_VERSION_MINOR "0" CACHE INTERNAL "framework minor version")
  endif()
  if(NOT DEFINED BDK_VERSION_PATCH)
    set(BDK_VERSION_PATCH "0" CACHE INTERNAL "framework patch version")
  endif()
  set(BDK_VERSION_STRING "${BDK_VERSION_MAJOR}.${BDK_VERSION_MINOR}.${BDK_VERSION_PATCH}" CACHE INTERNAL "bdk version string")

  # Get branch
  bdkGetGitTagOrBranch(_SOURCE_GIT_COMMIT_TAG_OR_BRANCH "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_TAG_OR_BRANCH ${_SOURCE_GIT_COMMIT_TAG_OR_BRANCH} CACHE INTERNAL "Source commit hash")

  bdkGetGitCommitHash(_SOURCE_GIT_COMMIT_HASH "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_HASH ${_SOURCE_GIT_COMMIT_HASH} CACHE INTERNAL "Source commit hash")

  # Get the latest commit datetime
  bdkGetGitCommitDateTime(_SOURCE_GIT_COMMIT_DATETIME "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_DATETIME ${_SOURCE_GIT_COMMIT_DATETIME} CACHE INTERNAL "Source commit datetime")

  # Get the build date time
  bdkGetBuildDateTime(_BDK_BUILD_DATETIME_UTC)
  set(BDK_BUILD_DATETIME_UTC ${_BDK_BUILD_DATETIME_UTC} CACHE INTERNAL "Build datetime UTC")


  ## Go to "${BDK_BSV_ROOT_DIR}" and find bsv repository informations:
  ##   Repo url
  ##   Repo branch
  ##   Repo git hash
  ##   Repo commit date time
  if(NOT (DEFINED BDK_BSV_ROOT_DIR AND EXISTS "${BDK_BSV_ROOT_DIR}"))
      message(FATAL_ERROR "Unable to find local bsv repository")
  endif()

  # Get branch
  bdkGetGitTagOrBranch(_BSV_GIT_COMMIT_TAG_OR_BRANCH "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_TAG_OR_BRANCH ${_BSV_GIT_COMMIT_TAG_OR_BRANCH} CACHE INTERNAL "BSV commit branch")

  bdkGetGitCommitHash(_BSV_GIT_COMMIT_HASH "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_HASH ${_BSV_GIT_COMMIT_HASH} CACHE INTERNAL "BSV commit hash")

  # Get the latest commit datetime
  bdkGetGitCommitDateTime(_BSV_GIT_COMMIT_DATETIME "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_DATETIME ${_BSV_GIT_COMMIT_DATETIME} CACHE INTERNAL "BSV commit datetime")

  bdkCalculateBSVVersion("${BDK_BSV_ROOT_DIR}/src/clientversion.h")
endfunction()
