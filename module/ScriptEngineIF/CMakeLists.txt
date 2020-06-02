#################################################################
#  Date             12/04/2019                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2019 nChain Limited. All rights reserved       #
#################################################################


set(ScriptEngineIF_HPP_FILES_PUBLIC # Keep alphabet order
  ${CMAKE_CURRENT_SOURCE_DIR}/ScriptEngineIF.h
)

set(ScriptEngineIF_HPP_FILES ${ScriptEngineIF_HPP_FILES_PUBLIC})

set(ScriptEngineIF_CPP_FILES # Keep alphabet order
  ${CMAKE_CURRENT_SOURCE_DIR}/ScriptEngineIF.cpp
)


set(ScriptEngineIF_SOURCE_FILES ${ScriptEngineIF_HPP_FILES} ${ScriptEngineIF_CPP_FILES})

add_library(ScriptEngineIF SHARED ${ScriptEngineIF_SOURCE_FILES})
if(WIN32) #https://github.com/openssl/openssl/pull/1062
  target_link_libraries(ScriptEngineIF crypt32 ws2_32)
  set_target_properties(ScriptEngineIF PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()
target_link_libraries(ScriptEngineIF ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} scrypt_core OpenSSL::Crypto OpenSSL::SSL)

target_include_directories(ScriptEngineIF PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET ScriptEngineIF PROPERTY FOLDER "module")

install(FILES ${ScriptEngineIF_HPP_FILES} DESTINATION "include/ScriptEngineIF" COMPONENT module)
install(TARGETS ScriptEngineIF DESTINATION "lib" COMPONENT module)
