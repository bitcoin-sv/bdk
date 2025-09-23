#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

find_package(Threads)
include(FindBoostHelper)
HelpFindBoost(filesystem program_options thread chrono)

foreach(comp IN LISTS "filesystem" "program_options" "thread" "chrono")
    if(NOT TARGET Boost::${comp})
        message(FATAL "Boost::${comp} is required but not found")
    endif()
endforeach()

add_executable(example_scriptengine_create_verify "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine_create_verify.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_scriptengine_create_verify PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_scriptengine_create_verify PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_scriptengine "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_scriptengine PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_scriptengine PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})