#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindBoostHelper_Include)
  return()
endif()
set(FindBoostHelper_Include TRUE)

#### Before building boost, need to install bzip2-devel, python-devel
#### Build boost
#### From boost 1.85.0, cmake doesn't maintain findpackage boost module
#### It is maintained by Boost themself. So the boost build have to deliver cmake modules
#### To get this, boost need to be build/install by cmake
####
####   Download the boost source version containing cmake build
####       https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.7z
####   Build command on linux
####       cmake ../boost_1_85_0_cmake -DCMAKE_INSTALL_PREFIX=/path/to/install/dir/boost_1_85_0_cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON && make -j8 && make install
####   Build command on Windows (same?)
####       cmake ..\boost_1_85_0_cmake -G"Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX=C:\Path\To\Install\Folder\boost_1_85_0_cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON && msbuild Boost.sln -maxcpucount:4 /p:Configuration=Debug && msbuild Boost.sln -maxcpucount:4 /p:Configuration=Release && cmake --install . --config Debug && cmake --install . --config Release
#### TODO better document how to fully build boost on Linux and Windows
####     Make sure special component like Boost::python, Boost::mpi (not priority) are built correctly
####
#### Note :
####     Linking with boost 1.86.0 works only on linux and windows. On Mac OS, the clang compiler doesn't
####     like the change of boost::uuid::uuid data type, which will fail due to the reinterpret_cast
####     in the file ${BDK_BSV_ROOT_DIR}/src/serialize.h line 989 and 999

#### Preset some variable to find and import boost dynamically
function(presetBoostVariable)##########################################################################
  set(boost_MINIMUM_REQUIRED 1.85 CACHE INTERNAL "Preset variable to find boost" FORCE)
  ## http://stackoverflow.com/questions/6646405/how-do-you-add-boost-libraries-in-cmakelists-txt
  if(NOT Boost_USE_STATIC_LIBS)
    set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Preset variable to find boost" FORCE)
  endif()
  if(NOT Boost_USE_STATIC_RUNTIME)
    set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "Preset variable to find boost" FORCE)
  endif()
  if(NOT Boost_USE_MULTITHREADED)
    set(Boost_USE_MULTITHREADED ON CACHE BOOL "Preset variable to find boost" FORCE)
  endif()

endfunction()

#### Linking with found boost
####     bdkLinkTargetToBoost(myTarget Boost::system Boost::random)
####     bdkLinkTargetToBoost(myTarget Boost::boost)## Header only
#### TODO Add a custom target Boost::ASIO and Boost::BEAST where import take all of openssl, and the header append the include of Boost::boost
macro(bdkLinkTargetToBoost)##################################################
  if(${ARGC} LESS 2)
    message(FATAL_ERROR "Error calling function bdkLinkTargetToBoost(myTarget Boost::comp1 Boost::compN)")
  endif()
  set(list_arg "${ARGN}")
  list(GET list_arg 0 _target) # First argument is the target
  list(REMOVE_AT list_arg 0)   # Remaining element in the list should be imported boost
  #bdkPrintList("Boost components to link" "${list_arg}")#Debug log

  if(NOT TARGET ${_target})
    message(FATAL_ERROR " [${_target}] is not a TARGET")
  endif()

  foreach(_imported_boost IN LISTS list_arg)
    if(NOT TARGET ${_imported_boost})
      message(FATAL_ERROR " [${_imported_boost}] was not an imported target")
    endif()
    target_link_libraries(${_target} PRIVATE ${_imported_boost})
  endforeach()

  if(UNIX)
    target_link_libraries(${_target} PUBLIC ${CMAKE_DL_LIBS})
  endif()
  
endmacro()

#### Boost imported target wrongly on windows by setting the .lib file for the property IMPORTED_LOCATION_<CONFIG>
#### Need to find the corresponding .dll file to copy and install to runtime directories

function(_CopyAndInstallSharedBoostComp _Boost_Comp)############################################################
  isStaticImported(_IS_STATIC_IMPORTED Boost ${_Boost_Comp})
  if(Boost_USE_STATIC_LIBS OR ${_IS_STATIC_IMPORTED})
    message(STATUS "  - [Boost::${_Boost_Comp}] is imported as static library, don't need a special fix")
    return()
  endif()

  if(CMAKE_CONFIGURATION_TYPES)
    set(_IMPORTED_LOCATION_PROPERTY_LIST IMPORTED_LOCATION_DEBUG IMPORTED_LOCATION_RELEASE)
  else()
    set(_IMPORTED_LOCATION_PROPERTY_LIST IMPORTED_LOCATION)
  endif()

  foreach(_IMPORTED_LOCATION_PROPERTY IN LISTS _IMPORTED_LOCATION_PROPERTY_LIST)
    get_property(_lib_File TARGET Boost::${_Boost_Comp} PROPERTY ${_IMPORTED_LOCATION_PROPERTY})
    if(NOT EXISTS ${_lib_File})
      message(FATAL_ERROR "Boost::${_Boost_Comp}::${_IMPORTED_LOCATION_PROPERTY} file [${_lib_File}] doesn't exist")
    endif()
    set(_dll_File "${_lib_File}")
    if(WIN32)## Fix imported location for boost, which should be dll instead of lib
      string(REPLACE ".lib" ".dll" _dll_File "${_lib_File}")
    endif()
    get_filename_component(_dll_EXT "${_dll_File}" EXT)
    if(${_dll_EXT} STREQUAL ".dll" OR ${_dll_EXT} STREQUAL ".dylib" OR ${_dll_EXT} MATCHES ".so")
      if(CMAKE_CONFIGURATION_TYPES)#config-like system
        string(REGEX REPLACE "IMPORTED_LOCATION_" "" _BUILD_TYPE "${_IMPORTED_LOCATION_PROPERTY}")
        bdkGetBuildTypeFromBUILDTYPE(_BuildType "${_BUILD_TYPE}")
        bdkCopyFileToBuildDir("${_dll_File}" ${_BuildType} ".")
      else()
        bdkCopyFileToBuildDir("${_dll_File}" "" ".")
      endif()
    else()
      message(FATAL_ERROR "Boost::${_Boost_Comp}::${_IMPORTED_LOCATION_PROPERTY}[${_lib_File}] is supposed to be imported dynamically")
    endif()
  endforeach()
endfunction()


#### HelpFindBoost help to find boost component providing the list of required component
####     HelpFindBoost(system random)
####     bdkLinkTargetToBoost(boost)## Header only
#### This will find the targets of format Boost::component to be linked
#### Note :
####  Call find_package(Threads) before calling HelpFindBoost might fix some warning
macro(HelpFindBoost)########################################################################################
  set(list_components "${ARGN}")

  if(POLICY CMP0144) ## Remove warning
    cmake_policy(SET CMP0144 NEW)
  endif()
  if(POLICY CMP0167) ## Remove warning
    cmake_policy(SET CMP0167 NEW)
  endif()

  presetBoostVariable()

  if(CUSTOM_BOOST_ROOT)
    set(BOOST_ROOT ${CUSTOM_BOOST_ROOT})
  elseif(DEFINED ENV{BOOST_ROOT})
    set(BOOST_ROOT $ENV{BOOST_ROOT})
  endif()

  message(STATUS "Try to find Boost in [${BOOST_ROOT}]")

  list(LENGTH list_components nb_comps)
  if(nb_comps LESS 1)
    if(TARGET Boost::boost)
      message(STATUS "Skip looking up for headers only Boost::boost as it was found")
    else()
      message(STATUS "Look up for headers only Boost::boost")
      find_package(Boost ${boost_MINIMUM_REQUIRED} REQUIRED)
    endif() 
  else()
    message(STATUS "Look up for boost components list ${list_components}")
    foreach(_Boost_Comp ${list_components})
      if(TARGET Boost::${_Boost_Comp})
        message(STATUS "Skip looking up for boost component Boost::${_Boost_Comp} as it was found")
      else()
        message(STATUS "  Look up for boost component Boost::${_Boost_Comp}")
        find_package(Boost ${boost_MINIMUM_REQUIRED} COMPONENTS ${_Boost_Comp} REQUIRED)

        ## Check if the component not static-imported
        isStaticImported(_IS_STATIC_IMPORTED Boost ${_Boost_Comp})
        if(${_IS_STATIC_IMPORTED})
          message(STATUS "  [Boost::${_Boost_Comp}] is imported as static, don't need to copy and install")
          continue()
        endif()

        if(NOT Boost_USE_STATIC_LIBS)
          bdkFixImportedTargetSymlink(Boost ${_Boost_Comp})
          _CopyAndInstallSharedBoostComp(${_Boost_Comp})
        endif()
        #TODO add message if BOOST_FOUND to show user how to add boost linking to a target
      endif()
    endforeach()
  endif()
endmacro()

#### bdkPrintBoostInfo print info of a list of found Boost::component
####     bdkPrintBoostInfo(filesystem program_options)
function(bdkPrintBoostInfo)
  if(${ARGC} LESS 1)
    message(FATAL_ERROR "Error calling function bdkPrintBoostInfo(comp1 compN), requires at least one argument")
  endif()

  if(NOT Boost_USE_STATIC_LIBS)##Nothing to do more if it is statically imported
    foreach(_Boost_Comp ${ARGN})
      bdkPrintProperties(Boost::${_Boost_Comp})
    endforeach()
  endif()
  bdkPrintProperties(Boost::boost)
  bdkPrintProperties(Boost::diagnostic_definitions)
  bdkPrintProperties(Boost::disable_autolinking)
  bdkPrintProperties(Boost::dynamic_linking)

  message(" --")
  message(" ---------- Boost_FOUND [${Boost_FOUND}]")
  message(" ---------- Boost_VERSION [${Boost_VERSION}]")
  message(" ---------- Boost_INCLUDE_DIRS [${Boost_INCLUDE_DIRS}]")
  message(" ---------- Boost_LIBRARIES [${Boost_LIBRARIES}]")
endfunction()
