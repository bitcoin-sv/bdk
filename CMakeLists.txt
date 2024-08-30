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

option(BDK_BUILD_MODULES "Activate the build of modules" ON)
option(BDK_BUILD_BSV_TESTS "Activate the build of bsv tests" ON)

# Language binding modules are all built by default. Activate one will deactivate other
option(BUILD_MODULE_JAVA "Build and test the Java module" ON)
option(BUILD_MODULE_PYTHON "Build and test the Python module" ON)
option(BUILD_MODULE_GOLANG "Build and test the Golang module" ON)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Hot fix when building on Mac OS, boost broke the build")
    #add_definitions(-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
    add_definitions(-DBOOST_NO_CXX98_FUNCTION_BASE)
endif()

## script engine built is based on source files from bsv code.
## Use -DBDK_BSV_SRC_ROOT=/path/to/sv_root
include(cmake/modules/FindBSVSourceHelper.cmake)
HelpFindBSVSource()
#bdkPrintBSVSourceInfo()#Print help to debug

## Global version should be the maximum of all version numbers
set(BDK_VERSION_MAJOR "1" CACHE INTERNAL "Script Engine SDK major version")
set(BDK_VERSION_MINOR "0" CACHE INTERNAL "Script Engine SDK minor version")
set(BDK_VERSION_PATCH "1" CACHE INTERNAL "Script Engine SDK patch version")

set(CMAKE_INCLUDE_CURRENT_DIR ON)## Always include the current directory

if(MSVC)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS -DNOMINMAX)
endif()

## Init CMake, find all external packages and eventually build them if necessary
include(cmake/BDKInit.cmake)
bdkInitCMake()

## Everything in core should be includable from everywhere
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)

## Build bdk_core
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core)

if(BDK_BUILD_MODULES)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/module)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/documentation)

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
