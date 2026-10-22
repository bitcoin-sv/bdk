#################################################################
#  Date             22/10/2026                                  #
#  Author           Chris Gibson                                #
#                                                               #
#  Copyright (c) 2026 nChain Limited. All rights reserved       #
#################################################################
if(BDKClangTidy_Include)## Include guard
  return()
endif()
set(BDKClangTidy_Include TRUE)

## clang-tidy on first-party C++ only, as part of the compile step.
##
## First-party and bitcoin-sv sources are compiled into the SAME targets (bdk_core
## via core/bdk-core-recipe.cmake; most example/test/gobdk/rust targets add
## BSV_APPLICATION_SRC_FILES directly), so a per-target CXX_CLANG_TIDY cannot
## isolate first-party code. Two mechanisms do it instead:
##   - sources: the SKIP_LINTING source property (CMake 3.27+), applied by a
##     deferred pass over every target once the whole tree is configured;
##   - headers: --header-filter / --exclude-header-filter (clang-tidy 19+), built
##     here from absolute paths because BSV_ROOT can live anywhere.
## Checks live in <repo>/.clang-tidy; WarningsAsErrors there fails the build.

## Escape a path for use inside a clang-tidy (POSIX ERE) regex
function(bdkClangTidyRegexEscape out_var input)
  string(REGEX REPLACE "([][.*+?^$(){}|\\\\])" "\\\\\\1" _escaped "${input}")
  set(${out_var} "${_escaped}" PARENT_SCOPE)
endfunction()

## Upstream-derived first-party files come from BDK_UPSTREAM_DERIVED_SOURCES
## (cmake/BDKBuildSetting.cmake); the ones that are #included by another source
## (the vendored ARM SHA-2 transforms) are also kept out of the header filter.

## TRUE when a compiled source must not be linted: bitcoin-sv (incl. secp256k1,
## univalue, leveldb, bsv tests), generated code, anything outside this checkout,
## or an upstream-derived file listed in BDK_UPSTREAM_DERIVED_SOURCES.
## `src` is absolute; _BDK_CLANG_TIDY_EXCLUDE_ABS is prepared by bdkEnableClangTidy().
function(bdkClangTidyIsExcluded out_var src)
  set(_excluded FALSE)
  cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${src}" NORMALIZE _in_checkout)
  cmake_path(IS_PREFIX BDK_BSV_ROOT_DIR "${src}" NORMALIZE _in_bsv)
  cmake_path(IS_PREFIX BDK_GENERATED_DIR "${src}" NORMALIZE _in_generated)
  if(NOT _in_checkout OR _in_bsv OR _in_generated)
    set(_excluded TRUE)
  elseif("${src}" IN_LIST _BDK_CLANG_TIDY_EXCLUDE_ABS)
    set(_excluded TRUE)
  endif()
  set(${out_var} ${_excluded} PARENT_SCOPE)
endfunction()

## All buildsystem targets created in `dir` and every directory below it
function(bdkClangTidyCollectTargets out_var dir)
  get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_sub IN LISTS _subdirs)
    bdkClangTidyCollectTargets(_sub_targets "${_sub}")
    list(APPEND _targets ${_sub_targets})
  endforeach()
  set(${out_var} ${_targets} PARENT_SCOPE)
endfunction()

## Deferred to the end of the root configure so every target already exists.
## Marks each excluded C++ source SKIP_LINTING in the directory that owns the
## target (source properties are directory-scoped), and switches clang-tidy off
## entirely on the third-party library targets.
function(bdkApplyClangTidyExclusions)
  foreach(_third_party secp256k1 univalue leveldb)
    if(TARGET ${_third_party})
      set_target_properties(${_third_party} PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endforeach()

  bdkClangTidyCollectTargets(_targets "${CMAKE_SOURCE_DIR}")
  set(_linted 0)
  set(_skipped 0)
  foreach(_target IN LISTS _targets)
    get_target_property(_type ${_target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
      continue()
    endif()
    get_target_property(_sources ${_target} SOURCES)
    get_target_property(_src_dir ${_target} SOURCE_DIR)
    if(NOT _sources)
      continue()
    endif()
    foreach(_src IN LISTS _sources)
      if(_src MATCHES "^\\$<")## generator expression: cannot be classified here
        continue()
      endif()
      get_filename_component(_abs "${_src}" ABSOLUTE BASE_DIR "${_src_dir}")
      get_filename_component(_ext "${_abs}" LAST_EXT)
      string(TOLOWER "${_ext}" _ext)
      if(NOT _ext MATCHES "^\\.(cpp|cxx|cc)$")## only CXX is tidied
        continue()
      endif()
      bdkClangTidyIsExcluded(_excluded "${_abs}")
      if(_excluded)
        set_source_files_properties("${_abs}" TARGET_DIRECTORY ${_target}
          PROPERTIES SKIP_LINTING ON)
        math(EXPR _skipped "${_skipped} + 1")
      else()
        math(EXPR _linted "${_linted} + 1")
      endif()
    endforeach()
  endforeach()
  message(STATUS "clang-tidy: ${_linted} first-party C++ sources linted, ${_skipped} third-party/generated sources skipped")
endfunction()

## Macro, not function: CMAKE_CXX_CLANG_TIDY and CMAKE_EXPORT_COMPILE_COMMANDS must
## land in the caller's (root) directory scope so every subdirectory inherits them.
## Call after bdkInitCMake() and BEFORE the first add_subdirectory().
macro(bdkEnableClangTidy)
  if(CMAKE_VERSION VERSION_LESS 3.27)
    message(FATAL_ERROR "BDK_ENABLE_CLANG_TIDY needs CMake >= 3.27 (SKIP_LINTING); found ${CMAKE_VERSION}. "
                        "Older CMake would silently lint bitcoin-sv as well.")
  endif()
  if(NOT DEFINED BDK_BSV_ROOT_DIR OR NOT DEFINED BDK_GENERATED_DIR)
    message(FATAL_ERROR "bdkEnableClangTidy() must run after HelpFindBSVSource() and bdkInitCMake()")
  endif()

  ## The pinned CI version first, then whatever the host calls its default
  find_program(BDK_CLANG_TIDY_EXE NAMES clang-tidy-23 clang-tidy
    DOC "clang-tidy executable used when BDK_ENABLE_CLANG_TIDY=ON")
  if(NOT BDK_CLANG_TIDY_EXE)
    message(FATAL_ERROR "BDK_ENABLE_CLANG_TIDY=ON but no clang-tidy found; install it or pass -DBDK_CLANG_TIDY_EXE=/path/to/clang-tidy")
  endif()
  execute_process(COMMAND "${BDK_CLANG_TIDY_EXE}" --version
    OUTPUT_VARIABLE _bdk_tidy_version_out OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(REGEX MATCH "LLVM version ([0-9]+)" _bdk_tidy_version_match "${_bdk_tidy_version_out}")
  set(_bdk_tidy_major "${CMAKE_MATCH_1}")
  if(NOT _bdk_tidy_major)
    message(FATAL_ERROR "Could not read the clang-tidy version from [${BDK_CLANG_TIDY_EXE} --version]:\n${_bdk_tidy_version_out}")
  endif()
  if(_bdk_tidy_major LESS 19)
    message(FATAL_ERROR "clang-tidy ${_bdk_tidy_major} is too old: --exclude-header-filter needs 19+ (CI pins 23)")
  endif()
  ## bitcoin-sv's thread-safety annotations use parameter packs that clang < 21
  ## rejects. bdkSetThreadSafetyAnnotations() neutralises them, but only when the
  ## compiler itself is clang; under GCC an old clang-tidy would still choke.
  if(_bdk_tidy_major LESS 21 AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "clang-tidy ${_bdk_tidy_major} with a ${CMAKE_CXX_COMPILER_ID} build may fail on bitcoin-sv's "
                    "thread-safety headers; use clang-tidy 21+ or build with clang (-DCMAKE_CXX_COMPILER=clang++)")
  endif()
  message(STATUS "clang-tidy enabled: ${BDK_CLANG_TIDY_EXE} (LLVM ${_bdk_tidy_major}); config ${CMAKE_SOURCE_DIR}/.clang-tidy")

  bdkClangTidyRegexEscape(_bdk_tidy_src_re "${CMAKE_SOURCE_DIR}")
  bdkClangTidyRegexEscape(_bdk_tidy_bsv_re "${BDK_BSV_ROOT_DIR}")

  ## The upstream-derived list feeds both exclusion mechanisms: SKIP_LINTING when
  ## such a file is compiled on its own, and the header filter when it is
  ## #included by another translation unit.
  set(_BDK_CLANG_TIDY_EXCLUDE_ABS "")
  set(_bdk_tidy_exclude_hdr_re "")
  foreach(_bdk_tidy_rel IN LISTS BDK_UPSTREAM_DERIVED_SOURCES)
    get_filename_component(_bdk_tidy_abs "${_bdk_tidy_rel}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${_bdk_tidy_abs}")
      message(FATAL_ERROR "BDK_UPSTREAM_DERIVED_SOURCES entry does not exist: ${_bdk_tidy_rel}")
    endif()
    list(APPEND _BDK_CLANG_TIDY_EXCLUDE_ABS "${_bdk_tidy_abs}")
    bdkClangTidyRegexEscape(_bdk_tidy_abs_re "${_bdk_tidy_abs}")
    string(APPEND _bdk_tidy_exclude_hdr_re "|${_bdk_tidy_abs_re}$")
  endforeach()

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)## lets `run-clang-tidy -p <build>` work too
  set(CMAKE_CXX_CLANG_TIDY
    "${BDK_CLANG_TIDY_EXE}"
    "--header-filter=^${_bdk_tidy_src_re}/(core|module|test)/"
    "--exclude-header-filter=^(${_bdk_tidy_bsv_re}/${_bdk_tidy_exclude_hdr_re})")

  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL bdkApplyClangTidyExclusions)
endmacro()
