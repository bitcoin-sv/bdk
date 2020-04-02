#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if(NOT Flutter_EXECUTABLE)
  message(FATAL_ERROR "Unable to finf Flutter_EXECUTABLE. Install flutter on your system")
endif()


set(BSCRYPT_IDE_CLIENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide CACHE INTERNAL "Directory containing flutter bscrypt_ide project")

set(PROTOBUF_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/lib/http_msg.pb.dart
  ${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/lib/http_msg.pbenum.dart
  ${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/lib/http_msg.pbjson.dart
  ${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/lib/http_msg.pbserver.dart
)
set_source_files_properties(${PROTOBUF_FILES} PROPERTIES GENERATED TRUE)

file(GLOB_RECURSE FLUTTER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/*.dart")

set(ALL_DART_FILES ${PROTOBUF_FILES} ${FLUTTER_SOURCE_FILES})

#foreach(_file ${PROTOBUF_FILES} ${FLUTTER_SOURCE_FILES})
#  message("------------------------------- dart file ${_file}")
#endforeach()
add_custom_target(client_ide ALL
                  COMMAND "${Flutter_EXECUTABLE}" pub get DEPENDS ${ALL_DART_FILES} WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}" COMMENT "Init flutter project : flutter pub get"
                  COMMAND "${Flutter_EXECUTABLE}" build web DEPENDS ${ALL_DART_FILES} WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}" COMMENT "Build web client : flutter build web"
                  SOURCES  ${ALL_DART_FILES}
)
add_dependencies(client_ide http_msg)

set_property(TARGET client_ide PROPERTY FOLDER "module/bscrypt_ide/client")


#################  ADD flutter test --machine TO CTEST ##########################

add_test(NAME flutter_test
         COMMAND "${Flutter_EXECUTABLE}" test
         WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}"
         COMMAND_EXPAND_LISTS
 )

#[[ TODO make it work for individual files so it can manage xml junit reporter
file(GLOB_RECURSE FLUTTER_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/test/*_test.dart")
foreach(_file ${FLUTTER_TEST_FILES})
  get_filename_component(_file_name_we ${_file} NAME_WE)
  set(_test_name dart_${_file_name_we})
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/bscrypt_ide/" "" _test_file "${_file}")
  ## message("------------------------------- _file ${_file}       _test_name[${_test_name}]     _test_file[${_test_file}]")

  if(CMAKE_CONFIGURATION_TYPES)
    set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
    ## Note at the moment, on Windows Py Module can be loaded only in release build. There are problem in debug build
    add_test(NAME ${_test_name} COMMAND "${Flutter_EXECUTABLE}" "test --machine ${_test_file} | tojunit --output ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${_test_name}.xml" CONFIGURATIONS Release WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}")
    add_test(NAME ${_test_name}${_tpost_fix} COMMAND "${Flutter_EXECUTABLE}" "test --machine  ${_test_file} | tojunit --output ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/${_test_name}${_tpost_fix}.xml" CONFIGURATIONS Debug WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}" COMMAND_EXPAND_LISTS)
  else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      set(_tpost_fix ${CMAKE_DEBUG_POSTFIX})
    endif()
    add_test(NAME ${_test_name}${_tpost_fix} COMMAND flutter test --machine ${_test_file} | tojunit --output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_test_name}${_tpost_fix}.xml" WORKING_DIRECTORY "${BSCRYPT_IDE_CLIENT_SOURCE_DIR}" COMMAND_EXPAND_LISTS)
  endif()
endforeach()
]]