#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

message("Compiling http_msg using generated protobuf files for IDE")

## Setting files path related to protobuf files
if(NOT protoFile)
  set(protoFile ${CMAKE_CURRENT_SOURCE_DIR}/http_msg.proto CACHE INTERNAL "Protobuf file")
  if(NOT EXISTS ${protoFile})
    message(FATAL_ERROR "protoFile [${protoFile}] Doesn't exist")
  endif()
endif()

## Get information about protobuf file (it's source directory and file name)
if(NOT protoc_SOURCE_DIR)
    get_filename_component(_protoc_SOURCE_DIR ${protoFile} DIRECTORY)
    set(protoc_SOURCE_DIR ${_protoc_SOURCE_DIR} CACHE INTERNAL "Directory containing protobuf file")
endif()
get_filename_component(protoc_FileName_WE ${protoFile} NAME_WE)

## Get information about output for protobuf generation files
set(protoc_OUT_DIR_CPP ${CMAKE_CURRENT_SOURCE_DIR}/server/src CACHE INTERNAL "Directory to include for generating protobuf c++ files")
set(protoc_OUT_DIR_DART ${CMAKE_CURRENT_SOURCE_DIR}/client/bscrypt_ide/lib CACHE INTERNAL "Directory to include for generating protobuf dart files")
foreach(_dir ${protoc_OUT_DIR_CPP} ${protoc_OUT_DIR_DART})## Prepare directories to generate protoc files
  if(NOT EXISTS ${_dir})
    file(MAKE_DIRECTORY ${_dir})
  endif()
endforeach()

set(_protoc_HPP_FILE ${protoc_OUT_DIR_CPP}/${protoc_FileName_WE}.pb.h)
set(_protoc_CPP_FILE ${protoc_OUT_DIR_CPP}/${protoc_FileName_WE}.pb.cc)
set(protoc_CPP_FILES ${_protoc_HPP_FILE} ${_protoc_CPP_FILE} CACHE INTERNAL "List of protoc cpp files")
set(_protoc_DART_FILE ${protoc_OUT_DIR_DART}/${protoc_FileName_WE}.pb.dart)
set(_protoc_DART_ENUM_FILE ${protoc_OUT_DIR_DART}/${protoc_FileName_WE}.pbenum.dart)
set(_protoc_DART_JSON_FILE ${protoc_OUT_DIR_DART}/${protoc_FileName_WE}.pbjson.dart)
set(_protoc_DART_SERVER_FILE ${protoc_OUT_DIR_DART}/${protoc_FileName_WE}.pbserver.dart)
set(protoc_DART_FILES ${_protoc_DART_FILE} ${_protoc_DART_ENUM_FILE} ${_protoc_DART_JSON_FILE} ${_protoc_DART_SERVER_FILE} CACHE INTERNAL "List of protoc dart files")
foreach(_file ${protoc_CPP_FILES} ${protoc_DART_FILES})## Log informations
  message("  protobuf generated file [${_file}]")
endforeach()

add_custom_command(OUTPUT ${protoc_CPP_FILES}
                   DEPENDS ${protoFile}
                   COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I=${protoc_SOURCE_DIR} --cpp_out=${protoc_OUT_DIR_CPP} ${protoFile} COMMENT "Generate ccp files for ${protoFile}"
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(OUTPUT ${protoc_DART_FILES}
                   DEPENDS ${protoFile}
                   COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I=${protoc_SOURCE_DIR} --dart_out=${protoc_OUT_DIR_DART} ${protoFile} COMMENT "Generate dart files for ${protoFile}"
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_source_files_properties(${protoc_CPP_FILES} ${protoc_DART_FILES} PROPERTIES GENERATED TRUE)

add_library(http_msg STATIC ${protoc_CPP_FILES} ${protoc_DART_FILES})
target_link_libraries(http_msg protobuf::libprotobuf)
set_property(TARGET http_msg PROPERTY FOLDER "module/bscrypt_ide")