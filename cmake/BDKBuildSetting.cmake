#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
if(BDKBuildSetting_Include)## Include guard
  return()
endif()
set(BDKBuildSetting_Include TRUE)

macro(bdkSetCompilationOptions)## This has to be macro instead of a function because of add_definitions scope is local
  if(NOT BDK_SET_COMPILATION_OPTIONS_DONE)
    ## Set common compile options
    set(CMAKE_CXX_STANDARD 20 CACHE INTERNAL "Use of C++20" FORCE)#TODO : change to C++20 when visual studio support
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE INTERNAL "Require C++ standard" FORCE)
    set(CMAKE_CXX_EXTENSIONS OFF CACHE INTERNAL "Do not use C++ extension" FORCE)
    set(CMAKE_DEBUG_POSTFIX d CACHE INTERNAL "Use d as debug postfix for  targets" FORCE)

    if (MSVC)
      bdk_add_compiler_flag(/Gm- /MP /wd4251 /wd4244 /wd4307)## TODO test if DNOMINMAX can move here
      ## Using MSVC STATIC RUNTIME)
      ##foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      ##  if(${flag_var} MATCHES "/MD")
      ##    string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      ##  endif(${flag_var} MATCHES "/MD")
      ##endforeach()
    endif()

    if(UNIX)
      set(CMAKE_CXX_FLAGS "-fPIC -fvisibility=default" CACHE STRING "compilation flags for C++" FORCE)
      set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 " CACHE STRING "flags for C++ debug version" FORCE)
      set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -DNDEBUG" CACHE STRING "flags for C++ release version" FORCE)
      set(CMAKE_C_FLAGS "-fPIC" CACHE STRING "flags for C" FORCE)
      set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "flags for C debug version" FORCE)
      set(CMAKE_C_FLAGS_RELEASE "-O3 -g0 -DNDEBUG" CACHE STRING "flags for C release version" FORCE)
    endif()

    set(BDK_SET_COMPILATION_OPTIONS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()
endmacro()

#### Define output directories according to ${CMAKE_CONFIGURATION_TYPES} or ${CMAKE_BUILD_TYPE}
#### Based on the defined 
####   ${BDK_SYSTEM_BUILD_ARCHI}
####   ${CMAKE_CONFIGURATION_TYPES}
####   ${CMAKE_BUILD_TYPE}
function(bdkSetOutputDirectories)

  if(BDK_SET_OUTPUT_DIRS_DONE)
    return()
  else()
    set(BDK_SET_OUTPUT_DIRS_DONE TRUE CACHE BOOL "Protect to call twices")
  endif()

  if(NOT DEFINED BDK_SYSTEM_BUILD_ARCHI)
    message("  SetBuildFolders : variable [BDK_SYSTEM_BUILD_ARCHI] is not defined")
    message(FATAL_ERROR "On Windows, [BDK_SYSTEM_BUILD_ARCHI] should be defined x64 or x86")
  endif()

  if(CMAKE_BUILD_TYPE)
    set(_BuildTypeList "" "${CMAKE_BUILD_TYPE}")
  else()
    set(_BuildTypeList "${CMAKE_CONFIGURATION_TYPES}")
  endif()

  set(_LIST_OUTPUT_TYPES RUNTIME LIBRARY ARCHIVE)

  foreach(_BuildType IN LISTS _BuildTypeList)
    if(_BuildType)
      string(TOUPPER ${_BuildType} _BUILD_TYPE)
      string(TOLOWER ${_BuildType} _build_type_dir)
      set(_BUILD_TYPE_TAG "_${_BUILD_TYPE}")
    else()
      set(_BUILD_TYPE_TAG "")
      string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type_dir)
    endif()

    set(_MAIN_OUTPUT_PATH "${CMAKE_BINARY_DIR}/x${BDK_SYSTEM_BUILD_ARCHI}/${_build_type_dir}")
    if(NOT EXISTS "${_MAIN_OUTPUT_PATH}")
      file(MAKE_DIRECTORY "${_MAIN_OUTPUT_PATH}")
    endif()
    foreach(_OUT_TYPE ${_LIST_OUTPUT_TYPES})
      set(CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG} "${_MAIN_OUTPUT_PATH}" CACHE PATH "Default path for runtime ouput directory" FORCE)
      bdkAppendToGlobalSet(bdkOUTPUT_DIRECTORIES "CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}=[${CMAKE_${_OUT_TYPE}_OUTPUT_DIRECTORY${_BUILD_TYPE_TAG}}]")
    endforeach()
  endforeach()

  #bdkPrintList("bdkOUTPUT_DIRECTORIES" "${bdkOUTPUT_DIRECTORIES}")## Debug Log
endfunction()

#### Get bsv versions in the "${BDK_BSV_ROOT_DIR}/src/clientversion.h"
function(bdkCalculateBSVVersion clientversionFile)###############################################################
  if(NOT EXISTS "${clientversionFile}")
    message(FATAL_ERROR "BSV version file [${clientversionFile}] does not exist")
  endif()

  file(STRINGS ${clientversionFile} _CLIENT_VERSION_MAJOR_TMP REGEX "^#define CLIENT_VERSION_MAJOR[ \t]+[0-9]+[ \t]*$")
  file(STRINGS ${clientversionFile} _CLIENT_VERSION_MINOR_TMP REGEX "^#define CLIENT_VERSION_MINOR[ \t]+[0-9]+[ \t]*$")
  file(STRINGS ${clientversionFile} _CLIENT_VERSION_REVISION_TMP REGEX "^#define CLIENT_VERSION_REVISION[ \t]+[0-9]+[ \t]*$")

  string(REPLACE "#define CLIENT_VERSION_MAJOR " "" _CLIENT_VERSION_MAJOR ${_CLIENT_VERSION_MAJOR_TMP})
  string(REPLACE "#define CLIENT_VERSION_MINOR " "" _CLIENT_VERSION_MINOR ${_CLIENT_VERSION_MINOR_TMP})
  string(REPLACE "#define CLIENT_VERSION_REVISION " "" _CLIENT_VERSION_REVISION ${_CLIENT_VERSION_REVISION_TMP})

  set(BSV_CLIENT_VERSION_MAJOR ${_CLIENT_VERSION_MAJOR} CACHE INTERNAL "BSV CLIENT_VERSION_MAJOR")
  set(BSV_CLIENT_VERSION_MINOR ${_CLIENT_VERSION_MINOR} CACHE INTERNAL "BSV CLIENT_VERSION_MINOR")
  set(BSV_CLIENT_VERSION_REVISION ${_CLIENT_VERSION_REVISION} CACHE INTERNAL "BSV CLIENT_VERSION_REVISION")

  set(BSV_VERSION_STRING "${BSV_CLIENT_VERSION_MAJOR}.${BSV_CLIENT_VERSION_MINOR}.${BSV_CLIENT_VERSION_REVISION}" CACHE INTERNAL "BSV version string")
endfunction()

function(bdkSetBuildVersion)
  ## Recompute on every configure. A cached guard froze the version strings and
  ## the git metadata at a build dir's first configure; a GLOBAL property resets
  ## each configure and still stops a second call within one.
  get_property(_build_version_done GLOBAL PROPERTY BDK_SET_BUILD_VERSION_DONE)
  if(_build_version_done)
    return()
  endif()
  set_property(GLOBAL PROPERTY BDK_SET_BUILD_VERSION_DONE TRUE)
  unset(BDK_SET_BUILD_VERSION_DONE CACHE)## left behind by configures that used the cached guard

  if(NOT DEFINED BDK_VERSION_MAJOR)
    set(BDK_VERSION_MAJOR "1" CACHE INTERNAL "framework major version")
  endif()
  if(NOT DEFINED BDK_VERSION_MINOR)
    set(BDK_VERSION_MINOR "0" CACHE INTERNAL "framework minor version")
  endif()
  if(NOT DEFINED BDK_VERSION_PATCH)
    set(BDK_VERSION_PATCH "0" CACHE INTERNAL "framework patch version")
  endif()
  set(BDK_VERSION_STRING "${BDK_VERSION_MAJOR}.${BDK_VERSION_MINOR}.${BDK_VERSION_PATCH}" CACHE INTERNAL "bdk version string")

  # Get branch
  bdkGetGitTagOrBranch(_SOURCE_GIT_COMMIT_TAG_OR_BRANCH "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_TAG_OR_BRANCH ${_SOURCE_GIT_COMMIT_TAG_OR_BRANCH} CACHE INTERNAL "Source commit hash")

  bdkGetGitCommitHash(_SOURCE_GIT_COMMIT_HASH "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_HASH ${_SOURCE_GIT_COMMIT_HASH} CACHE INTERNAL "Source commit hash")

  # Get the latest commit datetime
  bdkGetGitCommitDateTime(_SOURCE_GIT_COMMIT_DATETIME "${CMAKE_SOURCE_DIR}")
  set(SOURCE_GIT_COMMIT_DATETIME ${_SOURCE_GIT_COMMIT_DATETIME} CACHE INTERNAL "Source commit datetime")


  ## Go to "${BDK_BSV_ROOT_DIR}" and find bsv repository informations:
  ##   Repo url
  ##   Repo branch
  ##   Repo git hash
  ##   Repo commit date time
  if(NOT (DEFINED BDK_BSV_ROOT_DIR AND EXISTS "${BDK_BSV_ROOT_DIR}"))
      message(FATAL_ERROR "Unable to find local bsv repository")
  endif()

  # Get branch
  bdkGetGitTagOrBranch(_BSV_GIT_COMMIT_TAG_OR_BRANCH "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_TAG_OR_BRANCH ${_BSV_GIT_COMMIT_TAG_OR_BRANCH} CACHE INTERNAL "BSV commit branch")

  bdkGetGitCommitHash(_BSV_GIT_COMMIT_HASH "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_HASH ${_BSV_GIT_COMMIT_HASH} CACHE INTERNAL "BSV commit hash")

  # Get the latest commit datetime
  bdkGetGitCommitDateTime(_BSV_GIT_COMMIT_DATETIME "${BDK_BSV_ROOT_DIR}")
  set(BSV_GIT_COMMIT_DATETIME ${_BSV_GIT_COMMIT_DATETIME} CACHE INTERNAL "BSV commit datetime")

  bdkCalculateBSVVersion("${BDK_BSV_ROOT_DIR}/src/clientversion.h")

  ## Stamp the build time once per source revision: the BDK and BSV commits, including
  ## their _dirty state as of this configure, and SOURCE_DATE_EPOCH, so setting,
  ## changing or unsetting it re-stamps. Re-stamping on every configure would rewrite
  ## the generated version sources, and rebuild everything that depends on them, on
  ## every cmake rerun.
  set(_revision "${SOURCE_GIT_COMMIT_HASH}/${BSV_GIT_COMMIT_HASH}/$ENV{SOURCE_DATE_EPOCH}")
  if(NOT DEFINED CACHE{BDK_BUILD_DATETIME_UTC}
     OR NOT "${BDK_BUILD_DATETIME_REVISION}" STREQUAL "${_revision}")
    bdkGetBuildDateTime(_BDK_BUILD_DATETIME_UTC)
    set(BDK_BUILD_DATETIME_UTC ${_BDK_BUILD_DATETIME_UTC} CACHE INTERNAL "Build datetime UTC")
    set(BDK_BUILD_DATETIME_REVISION "${_revision}" CACHE INTERNAL "Source revision BDK_BUILD_DATETIME_UTC was stamped for")
  endif()

  ## Re-run the configure, and so recompute all of the above, when either checkout
  ## moves or the BSV version changes
  bdkConfigureDependsOnGitHead("${CMAKE_SOURCE_DIR}")
  bdkConfigureDependsOnGitHead("${BDK_BSV_ROOT_DIR}")
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${BDK_BSV_ROOT_DIR}/src/clientversion.h")
endfunction()

#### bitcoin-sv's threadsafety.h puts parameter packs inside clang thread-safety
#### attributes (e.g. ACQUIRE(ms...) on sync.h's scoped_lock). Clang before 21,
#### which includes AppleClang 17, rejects that as a hard error. The annotations
#### only feed -Wthread-safety analysis and do not change generated code, and
#### upstream already expands them to nothing under non-clang compilers. When the
#### compiler cannot parse them, force-include a header that pre-empts
#### threadsafety.h with that same non-clang branch, leaving the upstream sources
#### unchanged.
function(bdkSetThreadSafetyAnnotations)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    return()
  endif()

  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("
    struct __attribute__((capability(\"mutex\"))) M {
      void lock() __attribute__((acquire_capability()));
      void unlock() __attribute__((release_capability()));
    };
    template<class... Ms> struct __attribute__((scoped_lockable)) L {
      explicit L(Ms&... ms) __attribute__((acquire_capability(ms...))) {}
      ~L() __attribute__((release_capability())) {}
    };
    int main() { M m; L<M> l{m}; (void)l; return 0; }"
    BDK_CXX_THREAD_SAFETY_PACK_EXPANSION)
  if(BDK_CXX_THREAD_SAFETY_PACK_EXPANSION)
    return()
  endif()

  ## Take the macro list from upstream's own non-clang branch so it tracks upstream
  set(_upstream_hdr "${BDK_BSV_ROOT_DIR}/src/threadsafety.h")
  file(READ "${_upstream_hdr}" _content)
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${_upstream_hdr}")
  string(FIND "${_content}" "#else" _else_pos)
  if(_else_pos EQUAL -1)
    message(FATAL_ERROR "No non-clang branch found in ${_upstream_hdr}")
  endif()
  math(EXPR _branch_start "${_else_pos} + 5")
  string(SUBSTRING "${_content}" ${_branch_start} -1 _branch)
  string(FIND "${_branch}" "#endif" _endif_pos)
  if(_endif_pos EQUAL -1)
    message(FATAL_ERROR "Unterminated non-clang branch in ${_upstream_hdr}")
  endif()
  string(SUBSTRING "${_branch}" 0 ${_endif_pos} _branch)

  ## The extraction assumes the first #else/#endif pair is the __clang__ split.
  ## A nested #if upstream adds inside the branch would truncate it here, and the
  ## failure would only show up on clang < 21 as errors in unrelated sources.
  foreach(_required "#define ACQUIRE(" "#define GUARDED_BY(" "#define SCOPED_LOCKABLE")
    string(FIND "${_branch}" "${_required}" _required_pos)
    if(_required_pos EQUAL -1)
      message(FATAL_ERROR "Non-clang branch extracted from ${_upstream_hdr} lacks '${_required}'; "
                          "update bdkSetThreadSafetyAnnotations() for the new header layout")
    endif()
  endforeach()
  string(FIND "${_branch}" "#if" _nested_pos)
  if(NOT _nested_pos EQUAL -1)
    message(FATAL_ERROR "Non-clang branch extracted from ${_upstream_hdr} contains a nested #if; "
                        "update bdkSetThreadSafetyAnnotations() for the new header layout")
  endif()

  set(_override_hdr "${BDK_GENERATED_HPP_DIR}/bdk_threadsafety_off.h")
  file(WRITE "${_override_hdr}.tmp"
    "// Generated by BDK from the non-clang branch of ${_upstream_hdr}.\n"
    "// This compiler cannot parse its clang thread-safety annotations.\n"
    "#ifndef BITCOIN_THREADSAFETY_H\n"
    "#define BITCOIN_THREADSAFETY_H\n"
    "${_branch}"
    "#endif // BITCOIN_THREADSAFETY_H\n")
  ## Every C++ object depends on this header, so only touch it when its content
  ## changes; a fresh mtime on each configure would rebuild the whole tree
  execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_override_hdr}.tmp" "${_override_hdr}")
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${_override_hdr}\">")
  message(STATUS "Thread-safety annotations disabled: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} cannot expand parameter packs in them")
endfunction()

#### Align the per-byte loop that destroys a script, GCC only. bitcoin-sv's
#### prevector::erase pops a script one byte per iteration; GCC inlines that
#### loop into the std::vector<CTxIn> and std::vector<CTxOut> destructors
#### (17 bytes on x86-64, 20 on aarch64). Where the link puts those
#### destructors decides whether the loop crosses a 64-byte line: unchanged
#### code measured 11-16% slower validating large scripts on Zen 3 and Sapphire
#### Rapids when it did.
#### GCC aligns a loop under -falign-loops only when it expects at least
#### align-loop-iterations (default 4) iterations, and it skips this one;
#### --param=align-loop-iterations=1 makes it start the loop on a 32-byte
#### boundary, so in both destructors the loop stays in one line wherever the
#### function lands. This does not hold for every inlined copy: GCC still skips
#### loops whose block it estimates below align-threshold. On the validation
#### path that leaves EvalScript's copies, aligned by
#### bdkSetEvalScriptLoopAlignment() below.
#### CI disassembles both destructors and EvalScript and fails if that stops
#### holding (.github/scripts/check_loop_alignment.py).
#### Clang and AppleClang turn the loop into a single store (seen on Clang 23
#### and AppleClang 21), so there is nothing to align; MSVC, clang-cl and
#### Emscripten (ID Clang) are not GNU.
#### The real fix is in bitcoin-sv: drop this, BDK_ALIGN_LOOPS and the CI check
#### once prevector::erase stops looping per element for trivially
#### destructible types.
#### Kept out of bdkSetCompilationOptions(): its cached guard skips it on a
#### re-configure, and directory compile options are not cached.
function(bdkSetLoopAlignment)
  if(NOT BDK_ALIGN_LOOPS)
    message(STATUS "Loop alignment: off (BDK_ALIGN_LOOPS=OFF)")
    return()
  endif()
  set(_align_flags -falign-loops=32 --param=align-loop-iterations=1)
  set(_langs)
  set(_versions)
  foreach(_lang C CXX)
    if(CMAKE_${_lang}_COMPILER_ID STREQUAL "GNU")
      foreach(_flag IN LISTS _align_flags)
        add_compile_options("$<$<COMPILE_LANGUAGE:${_lang}>:${_flag}>")
      endforeach()
      list(APPEND _langs ${_lang})
      list(APPEND _versions "GCC ${CMAKE_${_lang}_COMPILER_VERSION}")
    endif()
  endforeach()
  if(_langs)
    list(JOIN _align_flags " " _flags_str)
    list(JOIN _langs "," _langs_str)
    list(REMOVE_DUPLICATES _versions)
    list(JOIN _versions ", " _versions_str)
    message(STATUS "Loop alignment: ${_flags_str} for ${_langs_str} (${_versions_str})")
  else()
    message(STATUS "Loop alignment: off (compiler is ${CMAKE_CXX_COMPILER_ID}, not GCC)")
  endif()
endfunction()

#### The same per-byte loop is inlined into EvalScript (bitcoin-sv
#### src/script/interpreter.cpp), where it destroys the script code after each
#### OP_CHECKSIG/OP_CHECKMULTISIG: one iteration per byte of script code per
#### signature check. Unaligned, the OP_CHECKSIG copy crossed a 64-byte line at
#### one of the two placements interpreter.cpp's 32-byte aligned .text can take,
#### measured 7-10% slower on large script code on Zen 3 and Sapphire Rapids.
#### GCC aligns a loop only when its header runs at least
#### count_max / align-threshold (default 100, i.e. 1% of the function's hottest
#### block) by its static estimate; EvalScript's copies sit at 0.07-0.57%.
#### 4000 aligns the OP_CHECKSIG copy and both OP_CHECKMULTISIG copies, with 2.7x
#### margin over the lowest (0.0665%, needs 1504). The fourth copy, on the
#### early-return path, is in a block GCC treats as cold and is never aligned at
#### any threshold; it runs at most once per failing script. interpreter.cpp
#### only.
#### Source properties are directory-scoped: call from the directory that builds
#### the bdk_core variant, after bdkApplySourceWarningPolicy() (which sets
#### COMPILE_OPTIONS on every bitcoin-sv source).
function(bdkSetEvalScriptLoopAlignment)
  if(NOT BDK_ALIGN_LOOPS OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    return()
  endif()
  set_property(SOURCE "${BDK_BSV_ROOT_DIR}/src/script/interpreter.cpp"
    APPEND PROPERTY COMPILE_OPTIONS --param=align-threshold=4000)
  message(STATUS "Loop alignment: --param=align-threshold=4000 for src/script/interpreter.cpp")
endfunction()

## First-party paths that are copies or close derivatives of upstream code. They
## are held to upstream's style, not ours, and every edit widens the diff against
## their origin, so both the warning policy below and clang-tidy treat them like
## bitcoin-sv itself. Relative to the checkout root.
set(BDK_UPSTREAM_DERIVED_SOURCES
  "test/core/script_tests_modified.cpp"      ## modified copy of bitcoin-sv src/test/script_tests.cpp
  "core/sha256_arm_shani.cpp"                ## vendored Bitcoin Core ARMv8 SHA-2 transforms
  "module/gobdk/bdkcgo/secp256k1_cloned.h"   ## copy of secp256k1.h ("DO NOT MODIFY")
  CACHE STRING "First-party files treated as upstream code by the warning policy and clang-tidy (paths relative to the checkout root)")

#### Per-source warning policy. Source properties are directory-scoped, so call
#### this in every directory that compiles first-party or bitcoin-sv sources,
#### before its targets are defined.
####   - first-party .cpp under core/, module/ and test/ (minus the upstream-derived
####     list) get bitcoin-sv's own warning set, plus -Werror with BDK_WARNINGS_AS_ERRORS;
####   - bitcoin-sv sources compiled directly into BDK targets keep upstream's
####     warning policy. They include upstream headers relative to their own
####     directory, so the SYSTEM include directories do not silence warnings
####     inside them (e.g. clang's -Wreturn-type on streams.h under NDEBUG, a
####     configuration upstream never builds).
function(bdkApplySourceWarningPolicy)
  if(MSVC)
    return()
  endif()
  get_property(_fp_all GLOBAL PROPERTY BDK_FIRST_PARTY_SRC_FILES)
  if(NOT _fp_all)
    file(GLOB_RECURSE _fp_all
         "${CMAKE_SOURCE_DIR}/core/*.cpp" "${CMAKE_SOURCE_DIR}/module/*.cpp" "${CMAKE_SOURCE_DIR}/test/*.cpp")
    foreach(_rel IN LISTS BDK_UPSTREAM_DERIVED_SOURCES)
      get_filename_component(_abs "${_rel}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
      if(NOT EXISTS "${_abs}")
        message(FATAL_ERROR "BDK_UPSTREAM_DERIVED_SOURCES entry does not exist: ${_rel}")
      endif()
      list(REMOVE_ITEM _fp_all "${_abs}")
    endforeach()
    set_property(GLOBAL PROPERTY BDK_FIRST_PARTY_SRC_FILES "${_fp_all}")
  endif()
  ## bitcoin-sv's C++ warning set (src/CMakeLists.txt upstream), -Werror as its CI adds it
  set(_fp_opts -Wall -Wextra -Wcast-align -Wformat -Wformat-security -Wshadow -Wvla -Wsuggest-override)
  ## Not yet clean; each is switched on in its own commit with the fixes it needs
  list(APPEND _fp_opts
    -Wno-unused-parameter
    -Wno-shadow
    -Wno-range-loop-construct
    -Wno-pessimizing-move
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-catch-value>")
  if(BDK_WARNINGS_AS_ERRORS)
    list(APPEND _fp_opts -Werror)
  endif()
  set_source_files_properties(${_fp_all} PROPERTIES COMPILE_OPTIONS "${_fp_opts}")

  get_property(_bsv_all GLOBAL PROPERTY BDK_BSV_ALL_SRC_FILES)
  if(NOT _bsv_all)
    file(GLOB_RECURSE _bsv_all "${BDK_BSV_ROOT_DIR}/src/*.cpp")
    set_property(GLOBAL PROPERTY BDK_BSV_ALL_SRC_FILES "${_bsv_all}")
  endif()
  set_source_files_properties(${_bsv_all} PROPERTIES COMPILE_OPTIONS "-Wno-return-type")
endfunction()
