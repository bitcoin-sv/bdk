#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
if(BDKTools_Include)## Include guard
  return()
endif()
set(BDKTools_Include TRUE)

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

##############################################
####  Tools are functions to be reusable  ####
## TODO add tests for all cmake functions here

# Allow to easily add flags for C and C++
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)
function(bdk_add_c_compiler_flag)
  foreach(f ${ARGN})
    CHECK_C_COMPILER_FLAG(${f} FLAG_IS_SUPPORTED)
    if(FLAG_IS_SUPPORTED)
      string(APPEND CMAKE_C_FLAGS " ${f}")
    endif()
  endforeach()
  set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
endfunction()

function(bdk_add_cxx_compiler_flag)
  foreach(f ${ARGN})
    CHECK_CXX_COMPILER_FLAG(${f} FLAG_IS_SUPPORTED)
    if(FLAG_IS_SUPPORTED)
      string(APPEND CMAKE_CXX_FLAGS " ${f}")
    endif()
  endforeach()
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
endfunction()

macro(bdk_add_compiler_flag)
  bdk_add_c_compiler_flag(${ARGN})
  bdk_add_cxx_compiler_flag(${ARGN})
endmacro()

macro(bdk_remove_compiler_flags)
  foreach(f ${ARGN})
    string(REGEX REPLACE "${f}( |^)" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REGEX REPLACE "${f}( |^)" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  endforeach()
endmacro()

#### Create a new version number based on a base number, with an increment
####      add_v = base_v + incr
#### Usage :
####     createIncrementVersion(add_v ${base_v} incr "major version of something")
function(createIncrementVersion add_v base_v incr cache_comment)
  math(EXPR add_vv ${base_v}+${incr})
  set(${add_v} ${add_vv} CACHE INTERNAL ${cache_comment})
endfunction()############################################################################

#### Print OS Info
function(bdkPrintOSInfo)##############################################################################
  #### TODO print more read-only information like compiler, default compiler flags ...
  message(STATUS "     OS Info:")
  message(STATUS "         BDK_SYSTEM_X64[${BDK_SYSTEM_X64}]")
  message(STATUS "         BDK_SYSTEM_X86[${BDK_SYSTEM_X86}]")
  message(STATUS "         BDK_SYSTEM_BUILD_ARCHI[${BDK_SYSTEM_BUILD_ARCHI}]")
  message(STATUS "         BDK_SYSTEM_OS[${BDK_SYSTEM_OS}]")
  message(STATUS "         BDK_GENERATED_HPP_DIR[${BDK_GENERATED_HPP_DIR}]")

  foreach(_LANG C CXX)
    message(STATUS "         CMAKE_${_LANG}_FLAGS[${CMAKE_${_LANG}_FLAGS}]")
    foreach(_BUILD_TYPE RELEASE DEBUG)
      message(STATUS "         CMAKE_${_LANG}_FLAGS_${_BUILD_TYPE}[${CMAKE_${_LANG}_FLAGS_${_BUILD_TYPE}}]")
    endforeach()
  endforeach()

endfunction()

#### Check if the shared library is imported
#
function(checkIfImported _Lib _Component)
  if(NOT TARGET ${_Lib}::${_Component})
    message(FATAL_ERROR "BDKTools.cmake::checkIfImported TARGET[${_Lib}::${_Component}] doesn't exist")
  endif()
endfunction()

#### Check if the target is static-imported
#### Will crash if the target is not imported
function(isStaticImported result _Lib _Component)
  if(NOT TARGET ${_Lib}::${_Component})
    message(FATAL_ERROR "BDKTools.cmake::isStaticImported TARGET[${_Lib}::${_Component}] doesn't exist")
  endif()

  get_property(_IMPORTED_TYPE TARGET ${_Lib}::${_Component} PROPERTY TYPE)
  if("${_IMPORTED_TYPE}" STREQUAL "STATIC_LIBRARY")
    set(${result} TRUE PARENT_SCOPE)
  else()
    set(${result} FALSE PARENT_SCOPE)
  endif()
endfunction()

#### Get the root directory to install
#### Usage :
####     bdkGetInstallRootDir(_install_root_dir)
function(bdkGetInstallRootDir result)
#[[ Usage of absolute path for install will not work at CPack time
  if(CMAKE_CONFIGURATION_TYPES)
    set(installDir \$<IF:\$<CONFIG:Debug>,${CMAKE_INSTALL_PREFIX}/x${BDK_SYSTEM_BUILD_ARCHI}/debug,${CMAKE_INSTALL_PREFIX}/x${BDK_SYSTEM_BUILD_ARCHI}/release>)
  else()
    string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type)
    set(installDir "${CMAKE_INSTALL_PREFIX}/x${BDK_SYSTEM_BUILD_ARCHI}/${_build_type}")
  endif()
  set(${result} "${installDir}" PARENT_SCOPE)
]]
  set(${result} "." PARENT_SCOPE)
endfunction()############################################################################

#### Function copy the file to build dirs
####    _BuildType could be "" Debug Release. _BuildType shouldn't be empty on config-like system
####    _subDir is the child dir, usually "."
#### Usage :
####    bdkCopyFileToBuildDir(${my_file_dll} Debug ".")
####    bdkCopyFileToBuildDir(${my_file_so} "" ".")
function(bdkCopyFileToBuildDir _file _BuildType _subDir)
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyFileToBuildDir file[${_file}] doesn't exist")
  endif()

  if(NOT _BuildType AND CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyInstallSharedLib _BuildType[${_BuildType}] empty should not be used on config-like system")
  endif()

  if(IS_SYMLINK ${_file})# symlink will not work to copy to runtime
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyInstallSharedLib _file[${_file}] is a symlink, unable to copy")
  endif()

  if(_BuildType)
    string(TOUPPER ${_BuildType} _BUILD_TYPE)
    set(copyDestination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_BUILD_TYPE}}/${_subDir}") ## Absolute Path
  else()
    set(copyDestination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_subDir}") ## Absolute Path
  endif()
  #message("COPY File [${_file}] to [${copyDestination}] for [${_BuildType}]")# Debug Log
  file(COPY "${_file}" DESTINATION ${copyDestination})
endfunction()

#### Function copy the file to build dirs, and install to Install dir
####    _BuildType could be "" Debug Release. _BuildType shouldn't be empty on config-like system
####    _subDir is the child dir, usually "."
#### Usage :
####    bdkCopyInstallSharedLib(${my_file_dll} Debug ".")
####    bdkCopyInstallSharedLib(${my_file_so} "" ".")
function(bdkCopyInstallSharedLib _file _BuildType _subDir)
  get_filename_component(_lib_EXT "${_file}" EXT)
  if(NOT (${_lib_EXT} STREQUAL ".dll" OR ${_lib_EXT} STREQUAL ".dylib" OR ${_lib_EXT} MATCHES ".so"))
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyInstallSharedLib _subDir[${_subDir}] _BuildType[${_BuildType}] File[${_file}] is not a shared library")
  endif()

  if(NOT _BuildType AND CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyInstallSharedLib _BuildType[${_BuildType}] empty should not be used on config-like system")
  endif()

  if(IS_SYMLINK ${_file})# symlink will not work to copy to runtime and install.
    message(FATAL_ERROR "BDKTools.cmake::bdkCopyInstallSharedLib _file[${_file}] is a symlink, it will not work")
  endif()

  if(_BuildType)
    bdkCopyFileToBuildDir(${_file} ${_BuildType} ${_subDir})
  else()
    bdkCopyFileToBuildDir(${_file} "" ${_subDir})
  endif()

  bdkGetInstallRootDir(instDestination)

  #message("install File [${_file}] to [${instDestination}] for [${_BuildType}]")# Debug Log
  if(_BuildType)
    install(FILES "${_file}" CONFIGURATIONS "${_BuildType}" DESTINATION "${instDestination}/lib" COMPONENT Files)
  else()
    install(FILES "${_file}" DESTINATION "${instDestination}/lib" COMPONENT Files)
  endif()
endfunction()


#### On linux, to make things easier, let the software link directly to the library instead of using the symlink
####   _Lib could be Boost, Qt5
####   _Component is the component of the library
####   _BuildType could be empty, Debug, Release
####   _PRPTY     could be LOCATION or IMPORTED_LOCATION
####   Example Boost::system, Qt5::Core ...
####     bdkSolveImportedSymlink(Boost system IMPORTED_LOCATION "")
####     bdkSolveImportedSymlink(Qt5 Core LOCATION Debug)
function(bdkSolveImportedSymlink _Lib _Component _PRPTY _BuildType)
  if(CMAKE_CONFIGURATION_TYPES)## If config-like system, do nothing
    return()
  endif()

  if(NOT ((${_PRPTY} STREQUAL "LOCATION") OR (${_PRPTY} STREQUAL "IMPORTED_LOCATION")))
    message(FATAL_ERROR "BDKTools.cmake::bdkSolveImportedSymlink, _PRPTY [${PRPTY}] should be LOCATION or IMPORTED_LOCATION")
  endif()

  checkIfImported("${_Lib}" "${_Component}")

  if(NOT _BuildType)
    set(TGT_PROPERTY ${_PRPTY})
  else()
    string(TOUPPER ${_BuildType} _BUILD_TYPE)
    if(NOT ((${_BUILD_TYPE} STREQUAL "DEBUG") OR (${_BUILD_TYPE} STREQUAL "RELEASE")))
      message(FATAL_ERROR "BDKTools.cmake::bdkSolveImportedSymlink, _BuildType[${_BuildType}] should be empty string, or Debug/Release")
    endif()
    set(TGT_PROPERTY ${_PRPTY}_${_BUILD_TYPE})
  endif()

  get_property(_lib_${_PRPTY} TARGET ${_Lib}::${_Component} PROPERTY ${TGT_PROPERTY})
  if(NOT _lib_${_PRPTY})
    message(STATUS "WARNING : BDKTools.cmake::bdkSolveImportedSymlink target property ${_Lib}::${_Component}::${TGT_PROPERTY} was not defined")
    return()
  endif()

  #message("    ----  BEFORE BDKTools.cmake::bdkSolveImportedSymlink ${TGT_PROPERTY}[${_lib_${_PRPTY}}]")
  if(IS_SYMLINK ${_lib_${_PRPTY}})
    get_filename_component(_solved_symlink_file ${_lib_${_PRPTY}} REALPATH)
    if(EXISTS ${_solved_symlink_file})
      set_target_properties(${_Lib}::${_Component} PROPERTIES ${TGT_PROPERTY} ${_solved_symlink_file})
    else()
      message(FATAL_ERROR "BDKTools.cmake::bdkSolveImportedSymlink, file [${_solved_symlink_file}] doesn't exist")
    endif()
  else()
    message(STATUS "WARNING : BDKTools.cmake::bdkSolveImportedSymlink ${_Lib}::${_Component}::${TGT_PROPERTY}[${_lib_${_PRPTY}}] is not a symlink")
  endif()
  #get_property(test_lib_${_PRPTY} TARGET ${_Lib}::${_Component} PROPERTY ${TGT_PROPERTY})
  #message("    ----  AFTER  BDKTools.cmake::solveImportedSymlink ${TGT_PROPERTY}[${test_lib_${_PRPTY}}]")
endfunction()

#### On linux, to make things easier, let the software link directly to the library instead of using the symlink
####   _Lib could be Boost, Qt5
####   _Component is the component of the library
####   Example Boost::system, Qt5::Core ...
####      bdkFixImportedTargetSymlink(Boost system)
####      bdkFixImportedTargetSymlink(Qt5 Core)
function(bdkFixImportedTargetSymlink _Lib _Component)
  if(CMAKE_CONFIGURATION_TYPES)## If config-like system, do nothing
    return()
  endif()
  checkIfImported("${_Lib}" "${_Component}")

  set(_BuildTypeList "" "${CMAKE_BUILD_TYPE}")##TODO change CMAKE_BUILD_TYPE to imported configuration property
  set(_PRPTYList "LOCATION" "IMPORTED_LOCATION")
  foreach(_PRPTY IN LISTS _PRPTYList)
    foreach(_BuildType IN LISTS _BuildTypeList)
      #message(" ++ CALL _PRPTY[${_PRPTY}]  _BuildType[${_BuildType}]")#Debug Log
      if(_BuildType)
        bdkSolveImportedSymlink(${_Lib} ${_Component} ${_PRPTY} ${_BuildType})
      else()
        bdkSolveImportedSymlink(${_Lib} ${_Component} ${_PRPTY} "")
      endif()
    endforeach()
  endforeach()
endfunction()

#### Append to a global set (CACHE) without duplicate. If the list is not created, it will be created in CACHE
#### Usage :
####     bdkAppendToGlobalSet(mylist "newElem")
function(bdkAppendToGlobalSet listVar newElem)
  if(NOT ${listVar})
    set(${listVar} "${newElem}" CACHE INTERNAL "Global List [${listVar}]" FORCE)
  else()
    if("${newElem}" IN_LIST ${listVar})
      message(STATUS " bdkAppendToGlobalSet WARNING : [${newElem}] already exist in list [${listVar}]")
    else()
      set(${listVar} "${${listVar}};${newElem}" CACHE INTERNAL "Global List [${listVar}]" FORCE)
    endif()
  endif()
endfunction()############################################################################


#### List all subdirectories (absolute path) given the parent directory
#### Usage :
####     bdkListSubDir(result ${VAR_HOLDING_PARENT_DIRECTORY})
function(bdkListSubDir result _parentdir)
  if(NOT IS_DIRECTORY ${_parentdir})
    message(FATAL_ERROR "[${_parentdir}] is not an existing directory")
  endif()

  file(GLOB_RECURSE children LIST_DIRECTORIES true RELATIVE ${_parentdir} ${_parentdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${_parentdir}/${child})
      list(APPEND dirlist "${_parentdir}/${child}")
    endif()
  endforeach()
  set(${result} ${dirlist} PARENT_SCOPE)
endfunction()############################################################################

#### Usage : Do not forget to put the double quote around the list variable
####     bdkPrintList("MY_LIST_VARIABLE" "${MY_LIST_VARIABLE}")
function(bdkPrintList varName listVar)
  if(NOT listVar)
    message(FATAL_ERROR "[${listVar}] variable is not defined")
  endif()
  message(STATUS "  Elements in [${varName}]:")
  foreach(_elem ${listVar})
    message(STATUS "    + [${_elem}]")
  endforeach()
endfunction()############################################################################

#### From RELEASE/DEBUG give Release/Debu
#### Usage :
####     bdkGetBuildTypeFromBUILDTYPE(BuildType DEBUG)
function(bdkGetBuildTypeFromBUILDTYPE BuildType BUILDTYPE)
  if(${BUILDTYPE} STREQUAL "RELEASE")
    set(${BuildType} Release PARENT_SCOPE)
    return()
  endif()
  if(${BUILDTYPE} STREQUAL "DEBUG")
    set(${BuildType} Debug PARENT_SCOPE)
    return()
  endif()
  message(FATAL_ERROR "BUILDTYPE[${BUILDTYPE}] Doesn't match RELEASE or DEBUG")
endfunction()############################################################################

#### Get The source Tag, if not exist, return branch from git
#### Usage :
####     bdkGetGitTagOrBranch(myVar)                             # will investigate in ${CMAKE_SOURCE_DIR}
####     bdkGetGitTagOrBranch(myVar "${OTHER_SOURCE_REPO_DIR}")
function(bdkGetGitTagOrBranch)
    list(LENGTH ARGN _NB_ARGS)
    list(GET ARGN 0 result)
    if(_NB_ARGS GREATER 1)
        list(GET ARGN 1 _CUSTOM_REPO)
        if(NOT EXISTS "${_CUSTOM_REPO}")
            message(FATAL_ERROR "bdkGetGitBranch call for inexisting directory [${_CUSTOM_REPO}]")
        endif()
    endif()

    if(EXISTS "${_CUSTOM_REPO}")
        set(_REPO_DIR ${_CUSTOM_REPO})
    else()
        set(_REPO_DIR ${CMAKE_SOURCE_DIR})
    endif()
    ### get git tag, if not exist, return branch
    # Get the current working branch
    execute_process(
      COMMAND /bin/sh -c "git describe --tags --exact-match $(git rev-parse HEAD) 2>/dev/null || git symbolic-ref --short HEAD"
      WORKING_DIRECTORY ${_REPO_DIR}
      OUTPUT_VARIABLE _Tag_Or_Branch
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(${result} ${_Tag_Or_Branch} PARENT_SCOPE)
endfunction()############################################################################

#### Get abreviation git commit hash of the last commit
#### Usage :
####     bdkGetGitCommitHash(myVar)                             # will investigate in ${CMAKE_SOURCE_DIR}
####     bdkGetGitCommitHash(myVar "${OTHER_SOURCE_REPO_DIR}")
function(bdkGetGitCommitHash)
    list(LENGTH ARGN _NB_ARGS)
    list(GET ARGN 0 result)
    if(_NB_ARGS GREATER 1)
        list(GET ARGN 1 _CUSTOM_REPO)
        if(NOT EXISTS "${_CUSTOM_REPO}")
            message(FATAL_ERROR "bdkGetGitBranch call for inexisting directory [${_CUSTOM_REPO}]")
        endif()
    endif()

    if(EXISTS "${_CUSTOM_REPO}")
        set(_REPO_DIR ${_CUSTOM_REPO})
    else()
        set(_REPO_DIR ${CMAKE_SOURCE_DIR})
    endif()

     # check if the working directory is dirty. If it is, set the commit hash to the string "dirty"
     execute_process(
       COMMAND git status --untracked-files=no --porcelain
       WORKING_DIRECTORY ${_REPO_DIR}
       OUTPUT_VARIABLE _is_Dirty_Working_Source
       OUTPUT_STRIP_TRAILING_WHITESPACE
     )

     # Get the latest abbreviated commit hash of the working branch
     execute_process(
       COMMAND git log -1 --format=%h
       WORKING_DIRECTORY ${_REPO_DIR}
       OUTPUT_VARIABLE _Source_Commit_Hash
       OUTPUT_STRIP_TRAILING_WHITESPACE
     )

    if(_is_Dirty_Working_Source)
      set(_Source_Commit_Hash "${_Source_Commit_Hash}_dirty")
    endif()

    set(${result} ${_Source_Commit_Hash} PARENT_SCOPE)
endfunction()############################################################################

#### Get last commit date time from git
#### Usage :
####     bdkGetGitCommitDateTime(myVar)                             # will investigate in ${CMAKE_SOURCE_DIR}
####     bdkGetGitCommitDateTime(myVar "${OTHER_SOURCE_REPO_DIR}")
function(bdkGetGitCommitDateTime)
    list(LENGTH ARGN _NB_ARGS)
    list(GET ARGN 0 result)
    if(_NB_ARGS GREATER 1)
        list(GET ARGN 1 _CUSTOM_REPO)
        if(NOT EXISTS "${_CUSTOM_REPO}")
            message(FATAL_ERROR "bdkGetGitBranch call for inexisting directory [${_CUSTOM_REPO}]")
        endif()
    endif()

    if(EXISTS "${_CUSTOM_REPO}")
        set(_REPO_DIR ${_CUSTOM_REPO})
    else()
        set(_REPO_DIR ${CMAKE_SOURCE_DIR})
    endif()

    # Get the latest commit datetime
    execute_process(
      COMMAND git log -1 --date=short --pretty=format:%ci
      WORKING_DIRECTORY ${_REPO_DIR}
      OUTPUT_VARIABLE _Source_commit_datetime
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${result} ${_Source_commit_datetime} PARENT_SCOPE)
endfunction()############################################################################

#### Get date time of the current moment
#### Usage :
####     bdkGetBuildDateTime(myVar)
function(bdkGetBuildDateTime result)
    string(TIMESTAMP _build_datetime_UTD "%d-%m-%Y %H:%M:%S")
    set(${result} ${_build_datetime_UTD} PARENT_SCOPE)
endfunction()############################################################################

#### Print a property value of a TARGET
#### Usage :
####     bdkPrintProperty(Boost::system IMPORTED_LOCATION_DEBUG)
function(bdkPrintProperty tgt prop)
  if(NOT TARGET ${tgt})
      message(FATAL_ERROR " bdkPrintProperty [${tgt}] is not a TARGET")
  endif()

  get_property(s TARGET ${tgt} PROPERTY ${prop} SET)
  get_property(d TARGET ${tgt} PROPERTY ${prop} DEFINED)
  if(s OR d)
    # v for value, d for defined, s for set
    get_property(v TARGET ${tgt} PROPERTY ${prop})
    # only produce output for values that are set
    message("       ${tgt}::${prop}=[${v}]")
  #else()
    #message("       ${tgt}::${prop}=[]   -- not SET nor DEFINED")
  endif()
endfunction()############################################################################

#### Print useful propertes value of a TARGET
#### Usage :
####     bdkPrintProperties(Boost::system)
function(bdkPrintProperties tgt)
  if(NOT TARGET ${tgt})
      message(FATAL_ERROR " bdkPrintProperties [${tgt}] is not a TARGET")
  endif()

  set(props ## TODO comment out (NOT REMOVE) the useless property from this list
      # Cmake 3.13 properties on target
      # https://cmake.org/cmake/help/v3.13/manual/cmake-properties.7.html
      ALIASED_TARGET
      ARCHIVE_OUTPUT_DIRECTORY_RELEASE
      ARCHIVE_OUTPUT_DIRECTORY_DEBUG
      ARCHIVE_OUTPUT_DIRECTORY
      ARCHIVE_OUTPUT_NAME_RELEASE
      ARCHIVE_OUTPUT_NAME_DEBUG
      ARCHIVE_OUTPUT_NAME
      AUTOGEN_BUILD_DIR
      AUTOGEN_PARALLEL
      AUTOGEN_TARGET_DEPENDS
      AUTOMOC_COMPILER_PREDEFINES
      AUTOMOC_DEPEND_FILTERS
      AUTOMOC_MACRO_NAMES
      AUTOMOC_MOC_OPTIONS
      AUTOMOC
      AUTOUIC
      AUTOUIC_OPTIONS
      AUTOUIC_SEARCH_PATHS
      AUTORCC
      AUTORCC_OPTIONS
      BINARY_DIR
      BUILD_RPATH
      BUILD_WITH_INSTALL_NAME_DIR
      BUILD_WITH_INSTALL_RPATH
      BUNDLE_EXTENSION
      BUNDLE
      C_EXTENSIONS
      C_STANDARD
      C_STANDARD_REQUIRED
      COMMON_LANGUAGE_RUNTIME
      COMPATIBLE_INTERFACE_BOOL
      COMPATIBLE_INTERFACE_NUMBER_MAX
      COMPATIBLE_INTERFACE_NUMBER_MIN
      COMPATIBLE_INTERFACE_STRING
      COMPILE_DEFINITIONS
      COMPILE_FEATURES
      COMPILE_FLAGS
      COMPILE_OPTIONS
      COMPILE_PDB_NAME
      COMPILE_PDB_NAME_RELEASE
      COMPILE_PDB_NAME_DEBUG
      COMPILE_PDB_OUTPUT_DIRECTORY
      COMPILE_PDB_OUTPUT_DIRECTORY_RELEASE
      COMPILE_PDB_OUTPUT_DIRECTORY_DEBUG
      RELEASE_OUTPUT_NAME
      DEBUG_OUTPUT_NAME
      RELEASE_POSTFIX
      CROSSCOMPILING_EMULATOR
      #CUDA_PTX_COMPILATION
      #CUDA_SEPARABLE_COMPILATION
      #CUDA_RESOLVE_DEVICE_SYMBOLS
      #CUDA_EXTENSIONS
      #CUDA_STANDARD
      #CUDA_STANDARD_REQUIRED
      CXX_EXTENSIONS
      CXX_STANDARD
      CXX_STANDARD_REQUIRED
      DEBUG_POSTFIX
      DEFINE_SYMBOL
      DEPLOYMENT_REMOTE_DIRECTORY
      DEPLOYMENT_ADDITIONAL_FILES
      DOTNET_TARGET_FRAMEWORK_VERSION
      EchoString
      ENABLE_EXPORTS
      EXCLUDE_FROM_ALL
      EXCLUDE_FROM_DEFAULT_BUILD_RELEASE
      EXCLUDE_FROM_DEFAULT_BUILD_DEBUG
      EXCLUDE_FROM_DEFAULT_BUILD
      EXPORT_NAME
      EXPORT_PROPERTIES
      FOLDER
      #Fortran_FORMAT
      #Fortran_MODULE_DIRECTORY
      FRAMEWORK
      FRAMEWORK_VERSION
      GENERATOR_FILE_NAME
      GNUtoMS
      HAS_CXX
      IMPLICIT_DEPENDS_INCLUDE_TRANSFORM
      IMPORTED_COMMON_LANGUAGE_RUNTIME
      IMPORTED_CONFIGURATIONS
      IMPORTED_GLOBAL
      IMPORTED_IMPLIB_RELEASE
      IMPORTED_IMPLIB_DEBUG
      IMPORTED_IMPLIB
      IMPORTED_LIBNAME_RELEASE
      IMPORTED_LIBNAME_DEBUG
      IMPORTED_LIBNAME
      IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE
      IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG
      IMPORTED_LINK_DEPENDENT_LIBRARIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE
      IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG
      IMPORTED_LINK_INTERFACE_LIBRARIES
      IMPORTED_LINK_INTERFACE_MULTIPLICITY_RELEASE
      IMPORTED_LINK_INTERFACE_MULTIPLICITY_DEBUG
      IMPORTED_LINK_INTERFACE_MULTIPLICITY
      IMPORTED_LOCATION_RELEASE
      IMPORTED_LOCATION_DEBUG
      IMPORTED_LOCATION
      IMPORTED_NO_SONAME_RELEASE
      IMPORTED_NO_SONAME_DEBUG
      IMPORTED_NO_SONAME
      IMPORTED_OBJECTS_RELEASE
      IMPORTED_OBJECTS_DEBUG
      IMPORTED_OBJECTS
      IMPORTED
      IMPORTED_SONAME_RELEASE
      IMPORTED_SONAME_DEBUG
      IMPORTED_SONAME
      IMPORT_PREFIX
      IMPORT_SUFFIX
      INCLUDE_DIRECTORIES
      INSTALL_NAME_DIR
      INSTALL_RPATH
      INSTALL_RPATH_USE_LINK_PATH
      INTERFACE_AUTOUIC_OPTIONS
      INTERFACE_COMPILE_DEFINITIONS
      INTERFACE_COMPILE_FEATURES
      INTERFACE_COMPILE_OPTIONS
      INTERFACE_INCLUDE_DIRECTORIES
      INTERFACE_LINK_DEPENDS
      INTERFACE_LINK_DIRECTORIES
      INTERFACE_LINK_LIBRARIES
      INTERFACE_LINK_OPTIONS
      INTERFACE_POSITION_INDEPENDENT_CODE
      INTERFACE_SOURCES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
      INTERPROCEDURAL_OPTIMIZATION_RELEASE
      INTERPROCEDURAL_OPTIMIZATION_DEBUG
      INTERPROCEDURAL_OPTIMIZATION
      IOS_INSTALL_COMBINED
      JOB_POOL_COMPILE
      JOB_POOL_LINK
      LABELS
      #<LANG>_CLANG_TIDY
      #<LANG>_COMPILER_LAUNCHER
      #<LANG>_CPPCHECK
      #<LANG>_CPPLINT
      #<LANG>_INCLUDE_WHAT_YOU_USE
      #<LANG>_VISIBILITY_PRESET
      LIBRARY_OUTPUT_DIRECTORY_RELEASE
      LIBRARY_OUTPUT_DIRECTORY_DEBUG
      LIBRARY_OUTPUT_DIRECTORY
      LIBRARY_OUTPUT_NAME_RELEASE
      LIBRARY_OUTPUT_NAME_DEBUG
      LIBRARY_OUTPUT_NAME
      LINK_DEPENDS_NO_SHARED
      LINK_DEPENDS
      LINKER_LANGUAGE
      LINK_DIRECTORIES
      LINK_FLAGS_RELEASE
      LINK_FLAGS_DEBUG
      LINK_FLAGS
      LINK_INTERFACE_LIBRARIES_RELEASE
      LINK_INTERFACE_LIBRARIES_DEBUG
      LINK_INTERFACE_LIBRARIES
      LINK_INTERFACE_MULTIPLICITY_RELEASE
      LINK_INTERFACE_MULTIPLICITY_DEBUG
      LINK_INTERFACE_MULTIPLICITY
      LINK_LIBRARIES
      LINK_OPTIONS
      LINK_SEARCH_END_STATIC
      LINK_SEARCH_START_STATIC
      LINK_WHAT_YOU_USE
      LOCATION_RELEASE
      LOCATION_DEBUG
      LOCATION
      MACOSX_BUNDLE_INFO_PLIST
      MACOSX_BUNDLE
      MACOSX_FRAMEWORK_INFO_PLIST
      MACOSX_RPATH
      MANUALLY_ADDED_DEPENDENCIES
      MAP_IMPORTED_CONFIG_RELEASE
      MAP_IMPORTED_CONFIG_DEBUG
      MAP_IMPORTED_CONFIG
      NAME
      NO_SONAME
      NO_SYSTEM_FROM_IMPORTED
      OSX_ARCHITECTURES_RELEASE
      OSX_ARCHITECTURES_DEBUG
      OSX_ARCHITECTURES
      OUTPUT_NAME_RELEASE
      OUTPUT_NAME_DEBUG
      OUTPUT_NAME
      PDB_NAME_RELEASE
      PDB_NAME_DEBUG
      PDB_NAME
      PDB_OUTPUT_DIRECTORY_RELEASE
      PDB_OUTPUT_DIRECTORY_DEBUG
      PDB_OUTPUT_DIRECTORY
      POSITION_INDEPENDENT_CODE
      PREFIX
      PRIVATE_HEADER
      PROJECT_LABEL
      PUBLIC_HEADER
      RESOURCE
      RULE_LAUNCH_COMPILE
      RULE_LAUNCH_CUSTOM
      RULE_LAUNCH_LINK
      RUNTIME_OUTPUT_DIRECTORY_RELEASE
      RUNTIME_OUTPUT_DIRECTORY_DEBUG
      RUNTIME_OUTPUT_DIRECTORY
      RUNTIME_OUTPUT_NAME_RELEASE
      RUNTIME_OUTPUT_NAME_DEBUG
      RUNTIME_OUTPUT_NAME
      SKIP_BUILD_RPATH
      SOURCE_DIR
      SOURCES
      SOVERSION
      STATIC_LIBRARY_FLAGS_RELEASE
      STATIC_LIBRARY_FLAGS_DEBUG
      STATIC_LIBRARY_FLAGS
      STATIC_LIBRARY_OPTIONS
      SUFFIX
      TYPE
      VERSION
      #VISIBILITY_INLINES_HIDDEN
      #VS_CONFIGURATION_TYPE
      #VS_DEBUGGER_COMMAND
      #VS_DEBUGGER_COMMAND_ARGUMENTS
      #VS_DEBUGGER_ENVIRONMENT
      #VS_DEBUGGER_WORKING_DIRECTORY
      #VS_DESKTOP_EXTENSIONS_VERSION
      #VS_DOTNET_REFERENCE_<refname>
      #VS_DOTNET_REFERENCEPROP_<refname>_TAG_<tagname>
      #VS_DOTNET_REFERENCES
      #VS_DOTNET_REFERENCES_COPY_LOCAL
      #VS_DOTNET_TARGET_FRAMEWORK_VERSION
      #VS_GLOBAL_KEYWORD
      #VS_GLOBAL_PROJECT_TYPES
      #VS_GLOBAL_ROOTNAMESPACE
      #VS_GLOBAL_<variable>
      #VS_IOT_EXTENSIONS_VERSION
      #VS_IOT_STARTUP_TASK
      #VS_KEYWORD
      #VS_MOBILE_EXTENSIONS_VERSION
      #VS_SCC_AUXPATH
      #VS_SCC_LOCALPATH
      #VS_SCC_PROJECTNAME
      #VS_SCC_PROVIDER
      #VS_SDK_REFERENCES
      #VS_USER_PROPS
      #VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION
      #VS_WINRT_COMPONENT
      #VS_WINRT_EXTENSIONS
      #VS_WINRT_REFERENCES
      WIN32_EXECUTABLE
      WINDOWS_EXPORT_ALL_SYMBOLS
      #XCODE_ATTRIBUTE_<an-attribute>
      #XCODE_EXPLICIT_FILE_TYPE
      #XCODE_PRODUCT_TYPE
      #XCODE_SCHEME_ADDRESS_SANITIZER
      #XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN
      #XCODE_SCHEME_THREAD_SANITIZER
      #XCODE_SCHEME_THREAD_SANITIZER_STOP
      #XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER
      #XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER_STOP
      #XCODE_SCHEME_DISABLE_MAIN_THREAD_CHECKER
      #XCODE_SCHEME_MAIN_THREAD_CHECKER_STOP
      #XCODE_SCHEME_MALLOC_SCRIBBLE
      #XCODE_SCHEME_MALLOC_GUARD_EDGES
      #XCODE_SCHEME_GUARD_MALLOC
      #XCODE_SCHEME_ZOMBIE_OBJECTS
      #XCODE_SCHEME_MALLOC_STACK
      #XCODE_SCHEME_DYNAMIC_LINKER_API_USAGE
      #XCODE_SCHEME_DYNAMIC_LIBRARY_LOADS
      #XCODE_SCHEME_EXECUTABLE
      #XCODE_SCHEME_ARGUMENTS
      #XCODE_SCHEME_ENVIRONMENT
      #XCTEST
)

  foreach(p ${props})
    bdkPrintProperty("${tgt}" "${p}")
  endforeach()
endfunction()############################################################################
