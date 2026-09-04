#################################################################
#  Date             29/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindPythonHelper)

if(NOT Python_EXECUTABLE)
  HelpFindPython()
  #bdkPrintPythonInfo()#Debug Log
endif()

## mkdocs and the markdown extensions mkdocs.yml enables have to come from the same python
## environment, so they are looked up together : HelpFindMkdocs keeps the first interpreter that
## can import all of them. It also reports what was missing where, and BDK_DOC_PYTHON_EXECUTABLE
## can be set on the cmake command line to point at a specific interpreter.
HelpFindMkdocs()

if(NOT mkdocs_FOUND)
  message(STATUS "  install them with : python -m pip install mkdocs pymdown-extensions plantuml_markdown")
  message(STATUS "  or point the build at the right interpreter with -DBDK_DOC_PYTHON_EXECUTABLE=/path/to/python")
  message(WARNING "Deactivate building documentation")
  return()
endif()

message(STATUS "Activate building documentation [${mkdocs_PYTHON_EXECUTABLE}]")#################


file(COPY ${BDK_ROOT_CMAKE_MODULE_PATH}/doc_tools/plantuml.jar DESTINATION ${BDK_GENERATED_BIN_DIR})
if(WIN32)
  file(COPY ${BDK_ROOT_CMAKE_MODULE_PATH}/doc_tools/plantuml.cmd DESTINATION ${BDK_GENERATED_BIN_DIR})
else()
  file(COPY ${BDK_ROOT_CMAKE_MODULE_PATH}/doc_tools/plantuml DESTINATION ${BDK_GENERATED_BIN_DIR} FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
endif()
set(BDK_CORE_MKDOCS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(_TARGET_NAME core_doc)
add_custom_target(${_TARGET_NAME} ALL)

set(_ENV_PATH $ENV{PATH})
if(WIN32)## Assuming that in Windows, it use MSVC, so it is CMAKE_CONFIGURATION_TYPES ON
  string(REPLACE ";" "\$<SEMICOLON>" ENV_PATH "${_ENV_PATH}")
  string(REPLACE "/" "\\\\" BDK_GENERATED_BIN_DIR_WINDOWS "${BDK_GENERATED_BIN_DIR}")
  string(REPLACE "/" "\\\\" BDK_GENERATED_DIR_WINDOWS "${BDK_GENERATED_DIR}/${_TARGET_NAME}")
  add_custom_command(TARGET ${_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E env PATH="${ENV_PATH}$<SEMICOLON>${BDK_GENERATED_BIN_DIR_WINDOWS}" ${mkdocs_COMMAND} build -c -d ${BDK_GENERATED_DIR_WINDOWS}
                    WORKING_DIRECTORY ${BDK_CORE_MKDOCS_DIR}
  )

  set_property(TARGET ${_TARGET_NAME} PROPERTY FOLDER "documentation")
else()
  add_custom_command(TARGET ${_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E env PATH=${_ENV_PATH}:${BDK_GENERATED_BIN_DIR} ${mkdocs_COMMAND} build -c -d ${BDK_GENERATED_DIR}/${_TARGET_NAME}
                    WORKING_DIRECTORY ${BDK_CORE_MKDOCS_DIR}
  )
endif()

install(DIRECTORY "${BDK_GENERATED_DIR}/${_TARGET_NAME}/" DESTINATION "Documentation/${_TARGET_NAME}" COMPONENT Documentation)
