#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
if(SESDKInit_Include) ## Include guard
  return()
endif()
set(SESDKInit_Include TRUE)

#####################################################
#### Initialize everything related to cmake here ####
#

#### Hold the directory containing this current script ####
set(SESDK_ROOT_CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Root directory of cmake modules")

#### Set all directories/subdirectories in this root to CMAKE_MODULE_PATH
macro(scryptInitModulePaths)
  list(APPEND CMAKE_MODULE_PATH "${SESDK_ROOT_CMAKE_MODULE_PATH}")
  scryptListSubDir(dirlist ${SESDK_ROOT_CMAKE_MODULE_PATH})
  list(APPEND CMAKE_MODULE_PATH ${dirlist})
endmacro()

#### Force the CMAKE_BUILD_TYPE on UNIX like (not visual studio) system AND user doesn't specify the build type
#### TODO add option build production vs dev. If prod --> default to release build, if dev --> default to debug build
function(scryptSetCMakeBuildType)
  if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "CMAKE_BUILD_TYPE default to Release on Unix system" FORCE)
    message(WARNING "CMAKE_BUILD_TYPE was not defined, forced to [${CMAKE_BUILD_TYPE}]")
  endif()
endfunction()

#### Set some technical value of the OS
macro(scryptGetOSInfo)
  ## Set x64/x86
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SESDK_SYSTEM_X64 ON)
    set(SESDK_SYSTEM_BUILD_ARCHI 64 CACHE STRING "System archi")
  else()
    set(SESDK_SYSTEM_X86 ON)
    set(SESDK_SYSTEM_BUILD_ARCHI 32 CACHE STRING "System archi")
  endif()

  ## SESDK_SYSTEM_OS used for package name.
  ## Users can define -DCUSTOM_SYSTEM_OS_NAME=Ubuntu for example to have the customized os name on the installer
  if(WIN32) #Windows
    set(SESDK_SYSTEM_OS Windows CACHE STRING "System OS type")
  else()    # UNIX
    if(APPLE)
        set(SESDK_SYSTEM_OS MacOS CACHE STRING "System OS type")
    else()
        set(SESDK_SYSTEM_OS Linux CACHE STRING "System OS type")
    endif()
  endif()
endmacro()

#### Create directory in binary tree to put all generated files
macro(scryptCreateGeneratedDir)
  set(SESDK_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated" CACHE STRING "Directory containing all generated files")
  file(MAKE_DIRECTORY "${SESDK_GENERATED_DIR}")
endmacro()

#### Create directory in binary tree to put all generated hpp files
macro(scryptCreateGeneratedHppDir)
  set(SESDK_GENERATED_HPP_DIR "${CMAKE_BINARY_DIR}/generated/hpp" CACHE STRING "Directory containing all generated hpp files")
  file(MAKE_DIRECTORY "${SESDK_GENERATED_HPP_DIR}")
  include_directories("${SESDK_GENERATED_HPP_DIR}")
endmacro()

#### Create directory in binary tree to put all generated tools files
macro(scryptCreateGeneratedToolsDir)
  set(SESDK_GENERATED_TOOLS_DIR "${CMAKE_BINARY_DIR}/generated/tools" CACHE STRING "Directory containing all generated tools")
  file(MAKE_DIRECTORY "${SESDK_GENERATED_TOOLS_DIR}")
  set(SESDK_GENERATED_BIN_DIR "${SESDK_GENERATED_TOOLS_DIR}/bin" CACHE STRING "Directory containing all executable utilities")
  file(MAKE_DIRECTORY "${SESDK_GENERATED_BIN_DIR}")
endmacro()

#### Check consistency of CMAKE_BUILD_TYPE vs CMAKE_CONFIGURATION_TYPES
####   On build system like vs or xcode, CMAKE_CONFIGURATION_TYPES is defined for both Debug and Release
####   On build system like Unix, CMAKE_BUILD_TYPE is defined indicating the build type
####   Not both of them are defined
macro(scryptTestBuildType)
  ## Set x64/x86
  if(CMAKE_BUILD_TYPE AND CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "Both CMAKE_BUILD_TYPE and CMAKE_CONFIGURATION_TYPES are defined, which is not expected")
  endif()
endmacro()

#### Force CMAKE_INSTALL_PREFIX if not defined
macro(scryptForceInstallDir)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/INSTALLATION" CACHE PATH "Cmake prefix" FORCE)
  message(STATUS "SESDK WARNING: Forced CMAKE_INSTALL_PREFIX[${CMAKE_INSTALL_PREFIX}]")
endmacro()

#### Initialize all setting for using CMake
macro(scryptInitCMake)

  scryptTestBuildType()
  scryptSetCMakeBuildType()
  scryptForceInstallDir()
  cmake_policy(SET CMP0057 NEW)# USE IN_LIST functionality, example: if(${var} IN_LIST MYVAR_LIST)
  cmake_policy(SET CMP0074 NEW)# Avoid warning when find_package
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)#
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_CMakeTargets")

  include(${SESDK_ROOT_CMAKE_MODULE_PATH}/SESDKTools.cmake)
  scryptInitModulePaths()
  #scryptPrintList("CMAKE_MODULE_PATH" "${CMAKE_MODULE_PATH}") ## Debug Log
  scryptGetOSInfo()
  scryptCreateGeneratedDir()
  scryptCreateGeneratedHppDir()
  scryptCreateGeneratedToolsDir()
  #scryptPrintOSInfo()#Debug Log

  include(SESDKBuildSetting)
  scryptSetCompilationOptions()
  scryptSetOutputDirectories()
  scryptSetBuildVersion()
  install(FILES "${SESDK_VERSION_HPP}" DESTINATION "include/core" COMPONENT Files)

  ## Precalculate variable for installation
  scryptGetInstallRootDir(_install_root_dir)
  set(SESDK_COMMON_INSTALL_PREFIX "${_install_root_dir}" CACHE PATH "Common directory used for installation")

  find_package(Threads)
  if(NOT Threads_FOUND)
    message(FATAL_ERROR "Unable to find Threads library")
  endif()

  include(FindOpenSSLHelper)
  HelpFindOpenSSL()
  #scryptPrintOpenSSLInfo()#Debug Log

  include(FindBoostHelper)
  HelpFindBoost()
  #scryptPrintProperties(Boost::program_options)

  include(FindPythonHelper)
  HelpFindPython()
  #scryptPrintPythonInfo()#Debug Log

  ## To call inside the java binding module, not here
  #include(FindJavaJDKHelper)
  #HelpFindJavaJDK()
  #scryptPrintJavaJDKInfo()#Debug Log

  enable_testing()
endmacro()