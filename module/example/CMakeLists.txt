#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

add_executable(example_scriptengine_create_verify "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine_create_verify.cpp")
target_link_libraries(example_scriptengine_create_verify ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_scriptengine_create_verify PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_scriptengine "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine.cpp")
target_link_libraries(example_scriptengine ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
bdkLinkTargetToBoost(example_scriptengine Boost::boost Boost::program_options)
set_target_properties(example_scriptengine PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})