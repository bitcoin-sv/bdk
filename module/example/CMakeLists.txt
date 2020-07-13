#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

add_executable(example "${CMAKE_CURRENT_SOURCE_DIR}/example.cpp")
target_link_libraries(example ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

include_directories(${CMAKE_SOURCE_DIR}/module/cpp)
add_executable(test_script_engine "${CMAKE_CURRENT_SOURCE_DIR}/test_script_engine.cpp")
target_link_libraries(test_script_engine ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(test_script_engine PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(test_tx_keys "${CMAKE_CURRENT_SOURCE_DIR}/test_tx_keys.cpp")
target_link_libraries(test_tx_keys ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(test_tx_keys PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

