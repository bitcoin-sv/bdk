#################################################################
#  Date             12/03/2019                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindProtobufHelper_Include)
  return()
endif()
set(FindProtobufHelper_Include TRUE)

## Build protobuf from source and install it locally to your machine.
## Go to a build directory, clone the source code from github
##    git clone https://github.com/protocolbuffers/protobuf.git protobuf_src
## On Windows with Visual Studio
##    cmake -G"Visual Studio 15 2017" -A x64 C:\path\to\source\protobuf_src\cmake -DCMAKE_INSTALL_PREFIX=C:\path\to\install\directory\protobuf -Dprotobuf_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release && cmake --build . --target INSTALL --config Debug && cmake --build . --target INSTALL --config Release
## On Linux
##    cmake /path/to/source/protobuf_src/cmake -DCMAKE_INSTALL_PREFIX=/path/to/install/directory/protobuf -Dprotobuf_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON; make -j4 ; make install
## Name the environment variable Protobuf_ROOT pointing to the directory where protobuf is installed
##
## To use wiht cmake, use the predefined function from module FindProtobuf.cmake::protobuf_generate
##    Preset the protobuf_generate_PROTOC_OUT_DIR to the desired directory
##    Preset the protobuf_generate_LANGUAGE to the desired language (cpp, js, python...)
##    Preset the protobuf_generate_TARGET to the single protobuf file .proto
##    or Preset the protobuf_generate_PROTOS to the list of protobuf files *.proto


#### Help to find Protobuf.
macro(HelpFindProtobuf)########################################################################################
  # find protobuf
  set(Protobuf_USE_STATIC_LIBS ON)
  find_package(Protobuf)
endmacro()

function(bdkPrintProtobufInfo)
  bdkPrintProperties(protobuf::libprotobuf)
  bdkPrintProperties(protobuf::libprotobuf-lite)
  bdkPrintProperties(protobuf::libprotoc)
  bdkPrintProperties(protobuf::protoc)
  message(" --")
  message(" ---------- Protobuf_FOUND              [${Protobuf_FOUND}]")
  message(" ---------- Protobuf_VERSION            [${Protobuf_VERSION}]")
  message(" ---------- Protobuf_INCLUDE_DIRS       [${Protobuf_INCLUDE_DIRS}]")
  message(" ---------- Protobuf_LIBRARIES          [${Protobuf_LIBRARIES}]")
  message(" ---------- Protobuf_PROTOC_LIBRARIES   [${Protobuf_PROTOC_LIBRARIES}]")
  message(" ---------- Protobuf_LITE_LIBRARIES     [${Protobuf_LITE_LIBRARIES}]")
  message(" ---------- Protobuf_PROTOC_EXECUTABLE  [${Protobuf_PROTOC_EXECUTABLE}]")
endfunction()
