#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

add_executable(example_evalscript "${CMAKE_CURRENT_SOURCE_DIR}/example_evalscript.cpp")
target_link_libraries(example_evalscript ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_evalscript PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

include_directories(${CMAKE_SOURCE_DIR}/module/cpp)
add_executable(example_bdk "${CMAKE_CURRENT_SOURCE_DIR}/example_bdk.cpp")
target_link_libraries(example_bdk ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_bdk PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_tx_keys "${CMAKE_CURRENT_SOURCE_DIR}/example_tx_keys.cpp")
target_link_libraries(example_tx_keys ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_tx_keys PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_verify_script "${CMAKE_CURRENT_SOURCE_DIR}/example_verify_script.cpp")
target_link_libraries(example_verify_script ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
bdkLinkTargetToBoost(example_verify_script Boost::boost Boost::program_options)
set_target_properties(example_verify_script PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_extendedtx "${CMAKE_CURRENT_SOURCE_DIR}/example_extendedtx.cpp")
target_link_libraries(example_extendedtx ${CMAKE_DL_LIBS} bdk_core OpenSSL::Crypto OpenSSL::SSL)
bdkLinkTargetToBoost(example_extendedtx Boost::boost Boost::program_options)
set_target_properties(example_extendedtx PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})