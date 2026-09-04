#################################################################
#  Date             12/04/2019                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindPythonHelper_Include)
  return()
endif()
set(FindPythonHelper_Include TRUE)

## Recommanded to install python3
## We need to have both python interpreter and dev lib. It is preferred to install both Python3
## To build Python C extension, use
##   Python_add_library(MyPy_MODULE_NAME SOURCE_FILE1.cpp SOURCE_FILE2.cpp)
##   target_link_libraries(MyPy_MODULE_NAME PUBLIC ${my_other_dependencies})    ## Note the PRIVATE keyword here is important
##   set_target_properties(MyPy_MODULE_NAME PROPERTIES FOLDER DEBUG_POSTFIX "") ## Its better for Python module to not have debug postfix. It might creat problem when importing it
##
## Make sure all the shared libraries that the MyPy_MODULE_NAME depend on are foundable in PATH
## For linux system, make sure an empty file __init__.py is present in the same directory containing PyModule
## Write a python script
##  #!/usr/bin/env python3
##  import sys
##  sys.path.append('/absolute/path/to/directory/containing/MyPy_MODULE_NAME')
##  import MyPy_MODULE_NAME
##  # Start to use module functionalities here

#### Help to find Python. It will prefer python3
macro(HelpFindPython)########################################################################################
  # find python interpreter and python dev libs
  find_package(Python COMPONENTS Interpreter Development)
endmacro()

## Import names (NOT distribution names) of the python modules mkdocs needs to render this
## project's documentation. Kept in sync with documentation/mkdocs.yml :
##   mkdocs             <- mkdocs
##   pymdownx           <- pymdown-extensions
##   plantuml_markdown  <- plantuml_markdown
set(BDK_DOC_PYTHON_MODULES mkdocs pymdownx plantuml_markdown)

#### Probe one interpreter : can it run mkdocs AND import every module in <modules> ?
##   <result> is set to TRUE/FALSE, <reason> to a human readable explanation when it is FALSE.
function(bdkProbeDocPython interpreter modules result reason)
  set(${result} FALSE PARENT_SCOPE)

  if(NOT interpreter OR NOT EXISTS "${interpreter}")
    set(${reason} "not an existing interpreter" PARENT_SCOPE)
    return()
  endif()

  ## Fast path : one single process importing everything we need. mkdocs.__main__ is what
  ## `python -m mkdocs` executes, so importing it proves the module is actually runnable and
  ## not just a leftover entry in the metadata.
  set(_imports "import mkdocs.__main__")
  foreach(_mod IN LISTS modules)
    string(APPEND _imports ", ${_mod}")
  endforeach()

  execute_process(COMMAND "${interpreter}" -c "${_imports}"
                  RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
  if(_rc STREQUAL "0")
    set(${result} TRUE PARENT_SCOPE)
    set(${reason} "" PARENT_SCOPE)
    return()
  endif()

  ## Slow path, only to build an actionable diagnostic : find out which ones are missing
  set(_missing "")
  foreach(_mod IN LISTS modules ITEMS mkdocs.__main__)
    execute_process(COMMAND "${interpreter}" -c "import ${_mod}"
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT _rc STREQUAL "0")
      list(APPEND _missing "${_mod}")
    endif()
  endforeach()

  string(REPLACE ";" " " _missing "${_missing}")
  set(${reason} "cannot import [${_missing}]" PARENT_SCOPE)
endfunction()

#### Collect, in order of preference, every python interpreter worth probing for mkdocs
function(bdkCollectDocPythonCandidates output)
  set(_candidates "")

  if(WIN32)
    set(_py_names python.exe python3.exe)
    set(_venv_subdir Scripts)
  else()
    set(_py_names python3 python)
    set(_venv_subdir bin)
  endif()

  ## 1. Explicit user override, always wins
  if(BDK_DOC_PYTHON_EXECUTABLE)
    list(APPEND _candidates "${BDK_DOC_PYTHON_EXECUTABLE}")
  endif()

  ## 2. The activated virtual environment, if any
  if(DEFINED ENV{VIRTUAL_ENV})
    foreach(_name IN LISTS _py_names)
      list(APPEND _candidates "$ENV{VIRTUAL_ENV}/${_venv_subdir}/${_name}")
    endforeach()
  endif()

  ## 3. The interpreter the rest of the build uses
  if(Python_EXECUTABLE)
    list(APPEND _candidates "${Python_EXECUTABLE}")
  endif()

  ## 4. Every interpreter reachable through PATH, in PATH order. This also covers the
  ##    interpreter of a virtual environment that is on PATH without being activated, which
  ##    is where a pip-installed mkdocs usually lives.
  file(TO_CMAKE_PATH "$ENV{PATH}" _path_dirs)
  foreach(_dir IN LISTS _path_dirs)
    foreach(_name IN LISTS _py_names)
      if(EXISTS "${_dir}/${_name}")
        list(APPEND _candidates "${_dir}/${_name}")
      endif()
    endforeach()
  endforeach()

  if(_candidates)
    list(REMOVE_DUPLICATES _candidates)
  endif()
  set(${output} "${_candidates}" PARENT_SCOPE)
endfunction()

#### Main function helping to find a usable mkdocs
##   Optional arguments : the import names of the modules to require, defaulting to
##   BDK_DOC_PYTHON_MODULES.
##
##   mkdocs and its markdown extensions must come from the SAME environment. Looking up the
##   `mkdocs` launcher with find_program() cannot guarantee that : the first launcher on PATH
##   may belong to an environment that has no extensions, or be a stale console script whose
##   site-packages are long gone (it is still executable, it just fails at import time). So
##   instead of trusting a launcher, probe interpreters and keep the first one that can import
##   the whole set, then drive it as `python -m mkdocs`.
##
##   On success sets : mkdocs_FOUND, mkdocs_PYTHON_EXECUTABLE, mkdocs_COMMAND (a list),
##                     mkdocs_VERSION and mkdocs_EXECUTABLE (the matching launcher, when there
##                     is one next to the interpreter).
function(HelpFindMkdocs)
  if(mkdocs_FOUND)
    return()
  endif()

  set(_modules ${ARGN})
  if(NOT _modules)
    set(_modules ${BDK_DOC_PYTHON_MODULES})
  endif()

  bdkCollectDocPythonCandidates(_candidates)

  set(_diagnostic "")
  foreach(_py IN LISTS _candidates)
    bdkProbeDocPython("${_py}" "${_modules}" _usable _reason)
    if(NOT _usable)
      list(APPEND _diagnostic "${_py} : ${_reason}")
      continue()
    endif()

    execute_process(COMMAND "${_py}" -m mkdocs --version
                    OUTPUT_VARIABLE _version_output
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(_version "unknown")
    if(_version_output MATCHES "version ([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
      set(_version "${CMAKE_MATCH_1}")
    endif()

    ## The launcher sitting next to the interpreter belongs to the same environment, so it is
    ## the one that matches. Reported for convenience, the build still goes through -m mkdocs.
    get_filename_component(_py_dir "${_py}" DIRECTORY)
    set(_launcher "")
    foreach(_name mkdocs mkdocs.exe)
      if(NOT _launcher AND EXISTS "${_py_dir}/${_name}")
        set(_launcher "${_py_dir}/${_name}")
      endif()
    endforeach()

    set(mkdocs_PYTHON_EXECUTABLE "${_py}" CACHE FILEPATH
        "Python interpreter providing mkdocs and the documentation markdown extensions" FORCE)
    set(mkdocs_COMMAND "${_py};-m;mkdocs" CACHE STRING
        "Command driving mkdocs, as a CMake list" FORCE)
    set(mkdocs_EXECUTABLE "${_launcher}" CACHE FILEPATH
        "mkdocs launcher belonging to mkdocs_PYTHON_EXECUTABLE" FORCE)
    set(mkdocs_VERSION "${_version}" CACHE STRING "mkdocs version" FORCE)
    set(mkdocs_FOUND TRUE CACHE INTERNAL "mkdocs and the documentation modules were found")
    mark_as_advanced(mkdocs_PYTHON_EXECUTABLE mkdocs_COMMAND mkdocs_EXECUTABLE mkdocs_VERSION)

    message(STATUS "Found mkdocs ${_version} : ${_py} -m mkdocs")
    return()
  endforeach()

  ## Nothing usable. Say exactly what was tried and what was missing, so the user does not have
  ## to guess which of their python environments the build was looking at.
  set(mkdocs_FOUND FALSE CACHE INTERNAL "mkdocs and the documentation modules were found")
  string(REPLACE ";" " " _modules_str "${_modules}")
  message(STATUS "No python environment provides mkdocs and [${_modules_str}]")
  foreach(_line IN LISTS _diagnostic)
    message(STATUS "  ${_line}")
  endforeach()
  if(NOT _candidates)
    message(STATUS "  no python interpreter found at all")
  endif()
endfunction()

function(HelpFindPythonPackage package_name output)# ouput TRUE/FALSE
  ## Optional 3rd argument : the interpreter to question. Defaults to the one mkdocs was found
  ## in, then to the build interpreter, so that the answer always refers to a real environment.
  set(_py "${ARGV2}")
  if(NOT _py)
    if(mkdocs_PYTHON_EXECUTABLE)
      set(_py "${mkdocs_PYTHON_EXECUTABLE}")
    else()
      set(_py "${Python_EXECUTABLE}")
    endif()
  endif()

  set(${output} FALSE PARENT_SCOPE)
  if(NOT _py OR NOT EXISTS "${_py}")
    return()
  endif()

  ## Accept either an import name or a distribution name, and do not depend on pip being
  ## installed in that environment.
  string(REPLACE "-" "_" _module "${package_name}")
  execute_process(COMMAND "${_py}" -c "import ${_module}"
                  RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)

  if(NOT _rc STREQUAL "0")
    execute_process(COMMAND "${_py}" -c
                    "import importlib.metadata as m; m.version('${package_name}')"
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
  endif()

  if(_rc STREQUAL "0")
    set(${output} TRUE PARENT_SCOPE)
  endif()
endfunction()

function(bdkPrintPythonInfo)
  bdkPrintProperties(Python::Interpreter)
  bdkPrintProperties(Python::Python)
  message(" --")
  message(" ---------- Python_FOUND [${Python_FOUND}]")
  message(" ---------- Python_EXECUTABLE [${Python_EXECUTABLE}]")
  message(" ---------- Python_Development_FOUND [${Python_Development_FOUND}]")
  message(" ---------- Python_INCLUDE_DIRS [${Python_INCLUDE_DIRS}]")
  message(" ---------- Python_LIBRARIES [${Python_LIBRARIES}]")
  message(" ---------- Python_VERSION_MAJOR [${Python_VERSION_MAJOR}]")
  message(" ---------- Python_VERSION_MINOR [${Python_VERSION_MINOR}]")
  message(" ---------- Python_VERSION_PATCH [${Python_VERSION_PATCH}]")
  message(" ---------- Python_VERSION       [${Python_VERSION}]")
endfunction()
