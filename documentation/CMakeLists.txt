#################################################################
#  Date             29/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

if(NOT Python_EXECUTABLE)
  include(FindPythonHelper)
  HelpFindPython()
  #bdkPrintPythonInfo()#Debug Log
endif()

if(NOT mkdocs_EXECUTABLE)
  HelpFindMkdocs()
  #message(STATUS "Found mkdocs mkdocs_EXECUTABLE[${mkdocs_EXECUTABLE}]")
endif()

if(NOT (mkdocs_EXECUTABLE AND Python_EXECUTABLE))
  message(STATUS "Missing python or mkdocs")
  message(WARNING "Deactivate building documentation")
  return()
endif()

HelpFindPythonPackage("mkdocs" has_mkdocs)
HelpFindPythonPackage("pymdown-extensions" has_pymdown_extensions)
HelpFindPythonPackage("plantuml_markdown" has_plantuml_markdown)

if(NOT (has_mkdocs AND has_pymdown_extensions AND has_plantuml_markdown))
  message(STATUS "Missing python's packages for documentation")
  message(STATUS "  python -m pip install mkdocs pymdown-extensions plantuml_markdown")
  message(WARNING "Deactivate building documentation")
  return()
endif()

message(STATUS "Activate building documentation")#################################


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
                    COMMAND ${CMAKE_COMMAND} -E env PATH="${ENV_PATH}$<SEMICOLON>${BDK_GENERATED_BIN_DIR_WINDOWS}" mkdocs build -c -d ${BDK_GENERATED_DIR_WINDOWS}
                    WORKING_DIRECTORY ${BDK_CORE_MKDOCS_DIR}
  )

  set_property(TARGET ${_TARGET_NAME} PROPERTY FOLDER "documentation")
else()
  add_custom_command(TARGET ${_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E env PATH=${_ENV_PATH}:${BDK_GENERATED_BIN_DIR} mkdocs build -c -d ${BDK_GENERATED_DIR}/${_TARGET_NAME}
                    WORKING_DIRECTORY ${BDK_CORE_MKDOCS_DIR}
  )
endif()

install(DIRECTORY "${BDK_GENERATED_DIR}/${_TARGET_NAME}/" DESTINATION "Documentation/${_TARGET_NAME}" COMPONENT Documentation)
