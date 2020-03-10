#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build univalue requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${SCRYPT_BSV_ROOT_DIR}/sv/src/univalue/CMakeLists.txt with slightl modifications
#######################################################################

set(SCRYPT_BSV_UNIVALUE_ROOT "${SCRYPT_BSV_ROOT_DIR}/src/univalue")

set(BSV_UNIVALUE_PUBLIC_HDR_FILES
  "${SCRYPT_BSV_UNIVALUE_ROOT}/include/univalue_escapes.h"
  "${SCRYPT_BSV_UNIVALUE_ROOT}/include/univalue.h"
)
set(BSV_UNIVALUE_PRIVATE_HDR_FILES
  "${SCRYPT_BSV_UNIVALUE_ROOT}/lib/univalue_utffilter.h"
)
set(BSV_UNIVALUE_HDR_FILES ${BSV_UNIVALUE_PUBLIC_HDR_FILES} ${BSV_UNIVALUE_PRIVATE_HDR_FILES})

set(BSV_UNIVALUE_SRC_FILES
  "${SCRYPT_BSV_UNIVALUE_ROOT}/lib/univalue.cpp"
  "${SCRYPT_BSV_UNIVALUE_ROOT}/lib/univalue_get.cpp"
  "${SCRYPT_BSV_UNIVALUE_ROOT}/lib/univalue_read.cpp"
  "${SCRYPT_BSV_UNIVALUE_ROOT}/lib/univalue_write.cpp"
  ## Total number of source files : 4
)

add_library(univalue ${BSV_UNIVALUE_HDR_FILES} ${BSV_UNIVALUE_SRC_FILES})
target_include_directories(univalue
                          PUBLIC "${SCRYPT_BSV_UNIVALUE_ROOT}/include"
                          PRIVATE "${SCRYPT_BSV_UNIVALUE_ROOT}/lib"
)
set_target_properties(univalue PROPERTIES  FOLDER "core" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

## Log list of univalues source files
message(STATUS "Build univalue source code in ${SCRYPT_BSV_UNIVALUE_ROOT}")
foreach(_univalue_file ${BSV_UNIVALUE_HDR_FILES} ${BSV_UNIVALUE_SRC_FILES})
  if(SCRYPT_LOG_BSV_FILES)
    message(STATUS "    [${_univalue_file}]")
  endif()
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_univalue_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${SCRYPT_BSV_UNIVALUE_ROOT} PREFIX "bitcoin SRC" FILES "${_univalue_file}")
  else()
    source_group(TREE ${SCRYPT_BSV_UNIVALUE_ROOT} PREFIX "bitcoin HDR" FILES "${_univalue_file}")
  endif()
endforeach()
###############################################

#######################################################################################################################
## Install univalue the *.h header files should be kept as univalue structure    ####################################
foreach(_univalue_pubhdr ${BSV_UNIVALUE_PUBLIC_HDR_FILES})
  install(FILES ${_univalue_pubhdr} DESTINATION "include/univalue" COMPONENT univalue)
endforeach()
install(TARGETS univalue DESTINATION "lib" COMPONENT univalue)
###############################################