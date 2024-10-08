#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build univalue requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${BDK_BSV_ROOT_DIR}/sv/src/univalue/CMakeLists.txt with slightl modifications
#######################################################################

set(BDK_BSV_UNIVALUE_ROOT "${BDK_BSV_ROOT_DIR}/src/univalue")

set(BSV_UNIVALUE_PUBLIC_HDR_FILES
  "${BDK_BSV_UNIVALUE_ROOT}/include/univalue_escapes.h"
  "${BDK_BSV_UNIVALUE_ROOT}/include/univalue.h"
)
set(BSV_UNIVALUE_PRIVATE_HDR_FILES
  "${BDK_BSV_UNIVALUE_ROOT}/lib/univalue_utffilter.h"
)
set(BSV_UNIVALUE_HDR_FILES ${BSV_UNIVALUE_PUBLIC_HDR_FILES} ${BSV_UNIVALUE_PRIVATE_HDR_FILES})

set(BSV_UNIVALUE_SRC_FILES
  "${BDK_BSV_UNIVALUE_ROOT}/lib/univalue.cpp"
  "${BDK_BSV_UNIVALUE_ROOT}/lib/univalue_get.cpp"
  "${BDK_BSV_UNIVALUE_ROOT}/lib/univalue_read.cpp"
  "${BDK_BSV_UNIVALUE_ROOT}/lib/univalue_write.cpp"
  ## Total number of source files : 4
)

add_library(univalue ${BSV_UNIVALUE_HDR_FILES} ${BSV_UNIVALUE_SRC_FILES})
target_include_directories(univalue
                          PUBLIC "${BDK_BSV_UNIVALUE_ROOT}/include"
                          PRIVATE "${BDK_BSV_UNIVALUE_ROOT}/lib"
)
set_target_properties(univalue PROPERTIES  FOLDER "core" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

## Log list of univalues source files
message(STATUS "Build univalue source code in ${BDK_BSV_UNIVALUE_ROOT}")
foreach(_univalue_file ${BSV_UNIVALUE_HDR_FILES} ${BSV_UNIVALUE_SRC_FILES})
  if(BDK_LOG_BSV_FILES)
    message(STATUS "    [${_univalue_file}]")
  endif()
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_univalue_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${BDK_BSV_UNIVALUE_ROOT} PREFIX "bitcoin SRC" FILES "${_univalue_file}")
  else()
    source_group(TREE ${BDK_BSV_UNIVALUE_ROOT} PREFIX "bitcoin HDR" FILES "${_univalue_file}")
  endif()
endforeach()
###############################################

#######################################################################################################################
## Install univalue the *.h header files should be kept as univalue structure    ####################################

set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}\n// Include files for univalue\n")
foreach(_univalue_pubhdr ${BSV_UNIVALUE_PUBLIC_HDR_FILES})
  install(FILES ${_univalue_pubhdr} DESTINATION "include/univalue" COMPONENT univalue)

  ## Calculate the relative path for include file in installer
  get_filename_component(_f ${_univalue_pubhdr} NAME)
  set(_install_rl "univalue/${_f}")
  set(UNIFIED_INSTALL_HEADERS_STR "${UNIFIED_INSTALL_HEADERS_STR}#include \"${_install_rl}\"\n")
endforeach()
install(TARGETS univalue DESTINATION "lib" COMPONENT univalue)
###############################################
