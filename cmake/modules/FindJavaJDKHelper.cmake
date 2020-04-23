#################################################################
#  Date             22/04/2020                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################
## Include guard
if(FindJavaJDKHelper_Include)
  return()
endif()
set(FindJavaJDKHelper_Include TRUE)

## This module help to find the JNI library and java executable installed on your system
##
## JNI_FOUND
## Java_JAVAC_EXECUTABLE
## Java_JAR_EXECUTABLE
##
## This script assume you've installed java JDK on your system

#### Main function helping to find JNI
macro(HelpFindJavaJDK)########################################################################################
  find_package(Java COMPONENTS Development)
  find_package(JNI)
endmacro()

#### Print found JavaJDK for debug
function(scryptPrintJavaJDKInfo)##############################################################################
  message(" --")
  message("         JNI_FOUND[${JNI_FOUND}]")
  message("           JNI_INCLUDE_DIRS[${JNI_INCLUDE_DIRS}]")
  message("           JNI_LIBRARIES[${JNI_LIBRARIES}]")
  message("         Java_FOUND[${Java_FOUND}]")
  message("           Java_VERSION_STRING[${Java_VERSION_STRING}]")
  message("           Java_JAVA_EXECUTABLE[${Java_JAVA_EXECUTABLE}]")
  message("           Java_JAVAC_EXECUTABLE[${Java_JAVAC_EXECUTABLE}]")
  message("           Java_JAR_EXECUTABLE[${Java_JAR_EXECUTABLE}]")
endfunction()