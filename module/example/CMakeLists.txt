#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

add_executable(example_bar "${CMAKE_CURRENT_SOURCE_DIR}/example_bar.cpp")
target_link_libraries(example_bar ${CMAKE_DL_LIBS} scrypt_core OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(example_bar PROPERTIES  FOLDER "module" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})