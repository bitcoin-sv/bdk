#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## How to build 
##
## Tool and dependencies requirements
##
## Run to build/test/install
##
## On Windows
##   cmake -G"Visual Studio 17 2022" -A x64 ..\bdk && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release && cpack -G NSIS -C Debug
##
## Windows alternative with MSBuild parallele:
##   cmake -G"Visual Studio 17 2022" -A x64 ..\bdk && msbuild bdk.sln -maxcpucount:4 /p:Configuration=Debug && msbuild bdk.sln -maxcpucount:4 /p:Configuration=Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release && cpack -G NSIS -C Release
##   cmake -G"Visual Studio 17 2022" -A x64 ..\bdk && msbuild bdk.sln -v:q -maxcpucount:4 /p:Configuration=Debug && msbuild bdk.sln  -v:q -maxcpucount:4 /p:Configuration=Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release && cpack -G NSIS -C Release
##
## On Linux (Ubuntu)
##   cmake ../bdk -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8 ; ctest ; make install; cpack -G TGZ
##   R_=buildrelease;D_=builddebug;OS_=Ubuntu; mkdir $R_ $D_;cd $R_; cmake ../../bdk -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;cd ../$D_;cmake ../../bdk -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;ctest;cd ../$R_;ctest;cpack -G TGZ;cd ../$D_;cpack -G TGZ;cd ..;
##
## Packaging :
##   Windows : cpack -G NSIS -C Release ## Require to install NSIS
##             cpack --config CPackSourceConfig.cmake -G ZIP
##   Linus   : cpack -G TGZ ## use at cmake time to get the precise os type installer name : -DCUSTOM_SYSTEM_OS_NAME=Ubuntu
##             cpack --config CPackSourceConfig.cmake -G TGZ

cmake_minimum_required(VERSION 3.16)
project(bdk)

option(BDK_LOG_BSV_FILES "Log used bitcoin sv file to build" ON)
option(BDK_BUILD_LEVELDB "Activate the build of leveldb" OFF)
option(BDK_BUILD_UNIVALUE "Activate the build of univalue" ON)

option(BDK_BUILD_CORE_ONLY "Build only core and deactivate everything else" OFF)
option(BDK_BUILD_MODULES "Activate the build of modules" ON)
option(BDK_BUILD_CORE_TESTS "Activate the build of core tests" ON)

# Language binding modules are all built by default. Activate one will deactivate other
option(BUILD_MODULE_GOLANG "Build and test the Golang module" ON)
option(BUILD_MODULE_GOLANG_INSTALL_INSOURCE "Install the static stand alone GoBDK in source directory (module/gobdk)" ON)


if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Fix when building on Mac OS, boost broke the build")
    #add_definitions(-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
    add_definitions(-DBOOST_NO_CXX98_FUNCTION_BASE)
endif()

## BDK built is based on source files from bsv code.
## Use -DBDK_BSV_SRC_ROOT=/path/to/sv_root
include(cmake/modules/FindBSVSourceHelper.cmake)
HelpFindBSVSource()
#bdkPrintBSVSourceInfo()#Print help to debug

## Global version should be the maximum of all version numbers
set(BDK_VERSION_MAJOR "1" CACHE INTERNAL "Bitcoin Development Kit major version")
set(BDK_VERSION_MINOR "2" CACHE INTERNAL "Bitcoin Development Kit minor version")
set(BDK_VERSION_PATCH "0" CACHE INTERNAL "Bitcoin Development Kit patch version")

set(CMAKE_INCLUDE_CURRENT_DIR ON)## Always include the current directory

if(MSVC)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS -DNOMINMAX -D_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING)
endif()

## Init CMake, find all external packages and eventually build them if necessary
include(cmake/BDKInit.cmake)
bdkInitCMake()

## Everything in core should be includable from everywhere
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)

## Build bdk_core
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core)

if(NOT BDK_BUILD_CORE_ONLY)
  if(BDK_BUILD_MODULES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/module)
  endif()

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/documentation)
else ()
  if(BDK_BUILD_MODULES)
    message(WARNING "BDK_BUILD_MODULES was set while using BDK_BUILD_CORE_ONLY. This is conflictual")
  endif()

  if(BDK_BUILD_CORE_TESTS)
    message(WARNING "BDK_BUILD_CORE_TESTS was set while using BDK_BUILD_CORE_ONLY. This is conflictual")
  endif()

  if(BUILD_MODULE_GOLANG)
    message(WARNING "BUILD_MODULE_GOLANG was set while using BDK_BUILD_CORE_ONLY. This is conflictual")
  endif()

  if(BUILD_MODULE_GOLANG_INSTALL_INSOURCE)
    message(WARNING "BUILD_MODULE_GOLANG_INSTALL_INSOURCE was set while using BDK_BUILD_CORE_ONLY. This is conflictual")
  endif()
endif()

## On Widows, need to download NSIS https://nsis.sourceforge.io/Download
## Possible to do WIX generator for MSI installer
set(BDK_CPACK_CONFIG_IN "${CMAKE_SOURCE_DIR}/cmake/BDKCPackConfig.cpack.in")
set(BDK_CPACK_CONFIG "${CMAKE_BINARY_DIR}/BDKCPackConfig.cpack")
configure_file(${BDK_CPACK_CONFIG_IN} ${BDK_CPACK_CONFIG} @ONLY)
install(FILES "ReleaseNote.md" DESTINATION "." COMPONENT Files)
install(FILES "LICENCE.txt" DESTINATION "." COMPONENT Files)
include(${BDK_CPACK_CONFIG})
include(CPack)
message("Compile with [${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}]")