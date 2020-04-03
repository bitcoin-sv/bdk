#################################################################
#  Date             13/03/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindFlutterHelper_Include)
  return()
endif()
set(FindFlutterHelper_Include TRUE)

#### Flutter web setup  https://flutter.dev/docs/get-started/web
##
#### Install regular flutter [https://flutter.dev/docs/get-started/install]
##     git clone https://github.com/flutter/flutter.git -b stable
##     cd flutter
##     bin/flutter doctor
##     #Add $flutter/bin to system path environment variable
##
#### Update to use flutter web [https://flutter.dev/docs/get-started/web]
##     flutter channel beta
##     flutter upgrade
##     flutter config --enable-web
##     cd flutter
##     git remote get-url origin
##   Install additional packages
##     flutter pub global activate protoc_plugin
##     flutter pub global activate junitreport
##     #Add Pub/Cache/bin to system path environment variable
##     flutter doctor

#### Help to find Flutter
macro(HelpFindFlutter)########################################################################################
  # find flutter executable
  if(WIN32)
    find_program(_Flutter_EXECUTABLE flutter.bat)
    file(TO_NATIVE_PATH "${_Flutter_EXECUTABLE}" Flutter_EXECUTABLE)
  else()
    find_program(Flutter_EXECUTABLE flutter)
  endif()
  message("Found Flutter_EXECUTABLE [${Flutter_EXECUTABLE}]")
endmacro()

function(scryptPrintFlutterInfo)
  message(" ---------- Flutter_EXECUTABLE [${Flutter_EXECUTABLE}]")
endfunction()
