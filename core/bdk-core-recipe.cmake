#################################################################
#  Date             28/07/2026                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2026 nChain Limited. All rights reserved       #
#################################################################

## Reusable BDK core build recipe.
## Included by exactly two call sites, one per build tree: core/CMakeLists.txt
## (canonical native bdk_core) or module/typesbdk/wasm/CMakeLists.txt (the
## standalone bdk_core_wasm variant). Prerequisites at include time:
##   - HelpFindBSVSource() has run (BDK_BSV_ROOT_DIR, BSV_MINIMAL_*_FILES, BSV_INCLUDE_DIRS)
##   - bdkInitCMake() has run (BDK_GENERATED_*_DIR, build settings)
##   - a Boost::boost target exists
## The includer's CMAKE_CURRENT_BINARY_DIR hosts the secp256k1 sub-build.
## NO include guard: sibling directory scopes cannot see each other's variables,
## so a guard variable would be useless; the side-effecting setting-*.cmake files
## carry defensive TARGET guards instead.

set(BDK_CORE_RECIPE_DIR "${CMAKE_CURRENT_LIST_DIR}")   ## == <repo>/core wherever included from

## bitcoin-config.h generation (location-independent: uses BDK_BSV_ROOT_DIR and
## BDK_GENERATED_HPP_DIR only)
include("${BDK_CORE_RECIPE_DIR}/setting-bitcoin.cmake")

## secp256k1 sub-build: instantiated once per build tree (TARGET-guarded)
include("${BDK_CORE_RECIPE_DIR}/setting-secp256k1.cmake")

if(BDK_BUILD_UNIVALUE)
  include("${BDK_CORE_RECIPE_DIR}/setting-univalue.cmake")
  set(UNIVALUE_LIB univalue)
else()
  set(UNIVALUE_LIB "")
endif()
## NOTE: setting-leveldb.cmake is deliberately NOT part of the recipe. No core
## variant links leveldb (the core link line is secp256k1/univalue/Boost only),
## so it stays a side build owned by the thin native core/CMakeLists.txt.

## Factory: build one core library variant from the curated recipe.
## The four parameters are the ONLY specialization surface. Any future
## specialization need must be expressed either (a) through these four
## parameters, or (b) as post-hoc target_* calls the caller applies to its own
## variant in its own directory, or (c) as a recipe-level change that benefits
## every variant. Adding a fifth parameter requires the same review bar as
## editing core itself.
## OpenSSL is deliberately NOT a parameter: discovery must run at root scope
## (imported targets are directory-scoped and sibling consumers link them
## directly), and linkage is fully expressible per variant -- an OpenSSL
## variant passes LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL, a no-OpenSSL
## variant simply omits them.
function(bdk_add_core_library target)
  cmake_parse_arguments(ARG "" ""
    "EXCLUDE_BSV_SOURCES;ADDITIONAL_SOURCES;LINK_LIBRARIES;COMPILE_DEFINITIONS" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "bdk_add_core_library: unknown arguments [${ARG_UNPARSED_ARGUMENTS}]")
  endif()

  #### Generate version C++  #####
  ## Same output path for every variant; identical content
  set(BDK_VERSION_CPP_IN "${BDK_CORE_RECIPE_DIR}/BDKVersion.cpp.in")
  set(BDK_VERSION_CPP "${BDK_GENERATED_CPP_DIR}/BDKVersion.cpp")
  configure_file(${BDK_VERSION_CPP_IN} ${BDK_VERSION_CPP})

  ## Local copies of the global curated lists; the caller's exclusions apply
  ## here only (a plain set() on a cache-variable name shadows it locally and
  ## never writes back to the cache)
  set(_bsv_src_files ${BSV_MINIMAL_SRC_FILES})
  set(_bsv_hdr_files ${BSV_MINIMAL_HDR_FILES})
  if(ARG_EXCLUDE_BSV_SOURCES)
    list(REMOVE_ITEM _bsv_src_files ${ARG_EXCLUDE_BSV_SOURCES})
  endif()

  ## any .cpp .hpp file added here will be added group with bsv file to build the core.
  ## core/ extras are pinned to the recipe dir, NOT to the caller's dir
  file(GLOB_RECURSE _core_extra_hdr_files
       "${BDK_CORE_RECIPE_DIR}/*.hpp" "${BDK_CORE_RECIPE_DIR}/*.h")
  file(GLOB_RECURSE _core_extra_src_files "${BDK_CORE_RECIPE_DIR}/*.cpp")
  list(APPEND _core_extra_src_files ${ARG_ADDITIONAL_SOURCES})

  ## Log list of bsv source files
  message(STATUS "Build core using bsv source code in ${BDK_BSV_ROOT_DIR}")
  foreach(_bitcoin_file ${_bsv_hdr_files} ${_bsv_src_files})
    if(BDK_LOG_BSV_FILES)
      message(STATUS "    [${_bitcoin_file}]")
    endif()
    ## Set the nice structure in IDE
    get_filename_component(_file_ext "${_bitcoin_file}" EXT)
    if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
      source_group(TREE ${BDK_BSV_ROOT_DIR} PREFIX "bitcoin SRC" FILES "${_bitcoin_file}")
    else()
      source_group(TREE ${BDK_BSV_ROOT_DIR} PREFIX "bitcoin HDR" FILES "${_bitcoin_file}")
    endif()
  endforeach()
  source_group("_generated" FILES "${BITCOIN_CONFIG_FILE}" "${BDK_VERSION_CPP}")

  message(STATUS "Extra c++ file in ${BDK_CORE_RECIPE_DIR}")
  foreach(_extra_file ${_core_extra_hdr_files} ${_core_extra_src_files})
    if(BDK_LOG_BSV_FILES)
      message(STATUS "      +[${_extra_file}]")
    endif()
  endforeach()

  ## Build the core variant as a library using bsv source code
  add_library(${target}
    ${BITCOIN_CONFIG_FILE} ${BDK_VERSION_CPP}
    ${_core_extra_hdr_files} ${_bsv_hdr_files}
    ${_core_extra_src_files} ${_bsv_src_files})
  target_include_directories(${target} PUBLIC ${BSV_INCLUDE_DIRS} "${BDK_CORE_RECIPE_DIR}")
  target_link_libraries(${target} PRIVATE
    secp256k1 ${UNIVALUE_LIB} Boost::boost ${ARG_LINK_LIBRARIES})
  target_compile_definitions(${target} PUBLIC HAVE_CONFIG_H)
  target_compile_definitions(${target} PRIVATE
    BOOST_ALL_NO_LIB BOOST_SP_USE_STD_ATOMIC BOOST_AC_USE_STD_ATOMIC
    ${ARG_COMPILE_DEFINITIONS})

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # remove warning on linux
    ## The source property is directory-scoped: it lands in the directory
    ## building this variant (the caller's scope)
    set_source_files_properties("${BDK_BSV_ROOT_DIR}/src/script/sign.cpp"
      PROPERTIES COMPILE_FLAGS "-Wno-stringop-overread")
  endif()
  if(WIN32)
    target_link_libraries(${target} PRIVATE Crypt32.lib Ws2_32)
  endif()
  set_property(TARGET ${target} PROPERTY FOLDER "core")

  ## Export the extra-header list for the caller's install/unified-header blocks
  set(BDK_CORE_EXTRA_HDR_FILES ${_core_extra_hdr_files} PARENT_SCOPE)
endfunction()
