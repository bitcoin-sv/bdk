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

#### Note that to install on build machine, it is recommended to install as
##        SYSTEM on slave windows
##        root   on slave linux
##   because those are users that's run the build job
##
#### Flutter web setup  https://flutter.dev/docs/get-started/web
##
#### Install regular flutter [https://flutter.dev/docs/get-started/install]
##     git clone https://github.com/flutter/flutter.git -b stable
##     # Define environment variable PUB_CACHE pointing to $flutter/.pub-cache  (do not forget the dot)
##     cd flutter
##     bin/flutter doctor
##     # to system path environment variable $flutter/bin $flutter/bin/cache/dart-sdk/bin $PUB_CACHE/bin
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
##     flutter doctor
##
#### TO Uninstall
##      remove the $flutter directory
##      remove the $PUB_CACHE directory
##      unset environemnt variable previously set during installation

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
