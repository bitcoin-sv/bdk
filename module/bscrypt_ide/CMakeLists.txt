#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindProtobufHelper)
HelpFindProtobuf()
#scryptPrintProtobufInfo()#Debug Log

include(FindFlutterHelper)
HelpFindFlutter()
#scryptPrintFlutterInfo()#Debug Log

message("Building bscrypt IDE")

include("${CMAKE_CURRENT_SOURCE_DIR}/http_msg.cmake")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/client)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/server)