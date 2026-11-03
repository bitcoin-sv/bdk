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
file(COPY ${BDK_ROOT_CMAKE_MODULE_PATH}/doc_tools/plantuml DESTINATION ${BDK_GENERATED_BIN_DIR} FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
set(BDK_CORE_MKDOCS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(_TARGET_NAME core_doc)
add_custom_target(${_TARGET_NAME} ALL)

set(_ENV_PATH $ENV{PATH})
add_custom_command(TARGET ${_TARGET_NAME} POST_BUILD
                  COMMAND ${CMAKE_COMMAND} -E env PATH=${_ENV_PATH}:${BDK_GENERATED_BIN_DIR} ${mkdocs_COMMAND} build -c -d ${BDK_GENERATED_DIR}/${_TARGET_NAME}
                  WORKING_DIRECTORY ${BDK_CORE_MKDOCS_DIR}
)

install(DIRECTORY "${BDK_GENERATED_DIR}/${_TARGET_NAME}/" DESTINATION "Documentation/${_TARGET_NAME}" COMPONENT Documentation)
