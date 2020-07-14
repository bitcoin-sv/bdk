#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

add_executable(example_evalscript "${CMAKE_CURRENT_SOURCE_DIR}/example_evalscript.cpp")
target_link_libraries(example_evalscript ${CMAKE_DL_LIBS} sesdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_evalscript PROPERTIES  FOLDER "module/example_evalscript" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

include_directories(${CMAKE_SOURCE_DIR}/module/cpp)
add_executable(example_script_engine "${CMAKE_CURRENT_SOURCE_DIR}/example_script_engine.cpp")
target_link_libraries(example_script_engine ${CMAKE_DL_LIBS} sesdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_script_engine PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_tx_keys "${CMAKE_CURRENT_SOURCE_DIR}/example_tx_keys.cpp")
target_link_libraries(example_tx_keys ${CMAKE_DL_LIBS} sesdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_tx_keys PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

