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

# Print the C and C++ compiler paths
message(STATUS "C Compiler           : ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler         : ${CMAKE_CXX_COMPILER}")

# Print the compiler ID and version
if(CMAKE_C_COMPILER)
  message(STATUS "C Compiler ID       : ${CMAKE_C_COMPILER_ID}")
  message(STATUS "C Compiler Version  : ${CMAKE_C_COMPILER_VERSION}")
endif()

if(CMAKE_CXX_COMPILER)
  message(STATUS "C++ Compiler ID     : ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "C++ Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# Print the standard libraries used
message(STATUS "CMAKE_CXX_FLAGS       : ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG : ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE : ${CMAKE_CXX_FLAGS_RELEASE}")

# Print target architecture information
if(DEFINED CMAKE_OSX_ARCHITECTURES)
  message(STATUS "CMAKE_OSX_ARCHITECTURES : ${CMAKE_OSX_ARCHITECTURES}")
else()
  # Try to detect using the host system
  message(STATUS "CMAKE_OSX_ARCHITECTURES is NOT explicitly set.")
  message(STATUS "CMAKE_SYSTEM_NAME           : ${CMAKE_SYSTEM_NAME}")
  message(STATUS "CMAKE_SYSTEM_PROCESSOR      : ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Print host information
message(STATUS "CMAKE_HOST_SYSTEM_NAME       : ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR  : ${CMAKE_HOST_SYSTEM_PROCESSOR}")

message(STATUS "Building helloworld.cpp")
add_executable(hello helloworld.cpp)