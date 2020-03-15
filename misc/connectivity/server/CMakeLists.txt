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
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt\misc\connectivity\server && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release
## Windows alternative with MSBuild parallele:
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt\misc\connectivity\server && msbuild server.sln -maxcpucount:4 /p:Configuration=Debug && msbuild server.sln -maxcpucount:4 /p:Configuration=Release
##   cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt\misc\connectivity\server && msbuild server.sln -v:q -maxcpucount:4 /p:Configuration=Debug && msbuild server.sln  -v:q -maxcpucount:4 /p:Configuration=Release
##
## On Linux (Ubuntu)
##   cmake ../bscrypt/misc/connectivity/server -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8
##   R_=buildrelease;D_=builddebug;OS_=Ubuntu; mkdir $R_ $D_;cd $R_; cmake ../../bscrypt/misc/connectivity/server -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;cd ../$D_;cmake ../../bscrypt/misc/connectivity/server -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=$OS_;time -p make -j8;
##

cmake_minimum_required(VERSION 3.16)

project(server)

set(CompilerFlags
  CMAKE_CXX_FLAGS
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_C_FLAGS
  CMAKE_C_FLAGS_DEBUG
  CMAKE_C_FLAGS_RELEASE
)
if(MSVC)
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /bigobj")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")

  add_compile_definitions(_WIN32_WINNT=0x0A00)# https://github.com/boostorg/asio/issues/81 and  https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?redirectedfrom=MSDN&view=vs-2019
endif()

find_package(OpenSSL REQUIRED)

set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Preset variable to find boost" FORCE)
set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "Preset variable to find boost" FORCE)
set(Boost_USE_MULTITHREADED ON CACHE BOOL "Preset variable to find boost" FORCE)
find_package(Boost COMPONENTS date_time REQUIRED)

add_executable(http http.cpp)
target_link_libraries(http Boost::boost Boost::date_time Boost::diagnostic_definitions)

add_executable(http_async http_async.cpp)
target_link_libraries(http_async Boost::boost Boost::date_time Boost::diagnostic_definitions)

add_executable(https server_certificate.hpp https.cpp)
target_link_libraries(https Boost::boost Boost::date_time Boost::diagnostic_definitions OpenSSL::Crypto OpenSSL::SSL)

add_executable(https_async server_certificate.hpp https_async.cpp)
target_link_libraries(https_async Boost::boost Boost::date_time Boost::diagnostic_definitions OpenSSL::Crypto OpenSSL::SSL)