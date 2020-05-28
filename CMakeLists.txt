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
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release
##
## Windows alternative with MSBuild parallele:
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt && msbuild bscrypt.sln -maxcpucount:4 /p:Configuration=Debug && msbuild bscrypt.sln -maxcpucount:4 /p:Configuration=Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt && msbuild bscrypt.sln -v:q -maxcpucount:4 /p:Configuration=Debug && msbuild bscrypt.sln  -v:q -maxcpucount:4 /p:Configuration=Release && ctest -C Debug && ctest -C Release && cpack -G NSIS -C Release
##
## On Linux (Ubuntu)
##   cmake ../bscrypt -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8 ; ctest ; make install; cpack -G TGZ
##   R_=buildrelease;D_=builddebug;OS_=Ubuntu; mkdir $R_ $D_;cd $R_; cmake ../../bscrypt -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;cd ../$D_;cmake ../../bscrypt -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;ctest;cd ../$R_;ctest;cpack -G TGZ;cd ../$D_;cpack -G TGZ;cd ..;
##
## Packaging :
##   Windows : cpack -G NSIS -C Release ## Require to install NSIS
##             cpack --config CPackSourceConfig.cmake -G ZIP
##   Linus   : cpack -G TGZ ## use at cmake time to get the precise os type installer name : -DCUSTOM_SYSTEM_OS_NAME=Ubuntu
##             cpack --config CPackSourceConfig.cmake -G TGZ

cmake_minimum_required(VERSION 3.16)
project(bscrypt)

option(SCRYPT_LOG_BSV_FILES "Log used bitcoin sv file to build" ON)
option(SCRYPT_BUILD_LEVELDB "Activate the build of leveldb" OFF)
option(SCRYPT_BUILD_UNIVALUE "Activate the build of univalue" ON)

option(SCRYPT_BUILD_MODULES "Activate the build of modules" ON)
option(SCRYPT_BUILD_BSV_TESTS "Activate the build of bsv tests" ON)

## bscrypt built is based on source files from bsv code.
## Use -DSCRYPT_BSV_SRC_ROOT=/path/to/sv_root
include(cmake/modules/FindBSVSourceHelper.cmake)
HelpFindBSVSource()
#scryptPrintBSVSourceInfo()#Print help to debug

set(SCRYPT_VERSION_MAJOR "0" CACHE INTERNAL "bscrypt major version")
set(SCRYPT_VERSION_MINOR "0" CACHE INTERNAL "bscrypt minor version")
set(SCRYPT_VERSION_PATCH "1" CACHE INTERNAL "bscrypt patch version")

set(CMAKE_INCLUDE_CURRENT_DIR ON)## Always include the current directory

if(MSVC)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS -DNOMINMAX)
endif()

## Init CMake, find all external packages and eventually build them if necessary
include(cmake/SCRYPTInit.cmake)
scryptInitCMake()

## Everything in core should be includable from everywhere
include_directories(${CMAKE_SOURCE_DIR}/core)

## Build scrypt_core
add_subdirectory(${CMAKE_SOURCE_DIR}/core)

if(SCRYPT_BUILD_MODULES)
  add_subdirectory(${CMAKE_SOURCE_DIR}/module)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/test)

## On Widows, need to download NSIS https://nsis.sourceforge.io/Download
## Possible to do WIX generator for MSI installer
set(SCRYPT_CPACK_CONFIG_IN "${CMAKE_SOURCE_DIR}/cmake/SCRYPTCPackConfig.cpack.in")
set(SCRYPT_CPACK_CONFIG "${CMAKE_BINARY_DIR}/SCRYPTCPackConfig.cpack")
configure_file(${SCRYPT_CPACK_CONFIG_IN} ${SCRYPT_CPACK_CONFIG} @ONLY)
install(FILES "ReleaseNote.md" DESTINATION "." COMPONENT Files)
install(FILES "LICENCE.txt" DESTINATION "." COMPONENT Files)
include(${SCRYPT_CPACK_CONFIG})
include(CPack)
message("Compile with [${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}]")