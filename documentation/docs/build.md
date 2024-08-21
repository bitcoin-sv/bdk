## Getting the source code
Building Script Engine SDK requires the source for both Script Engine SDK and bsv repositories. 

For nChain developers, the best way to do this is to have a BitBucket account setup with [ssh key access](https://confluence.atlassian.com/bitbucket/set-up-an-ssh-key-728138079.html). If other access is available, the source code should be organised as per the directory structure outlined in [directories structure](directories.md)

## Prerequisites
##### General
- [Python 3.12 (64-bit)](https://www.python.org/downloads/release/python-3125) or later
**Windows**: If using the executable installer, ensure the "Download debugging symbols" and the "Download debug binaries" installation options are selected. 
- The following python packages are needed to install and build documentation and to run tests.
```console
python -m pip install pytest junitparser mkdocs pymdown-extensions plantuml_markdown
```
- [CMake 3.30.2](https://cmake.org/download/) or later
- [Boost 1.86](https://www.boost.org/doc/libs/1_78_0/) or later.
- [OpenSSL 3.0.9](https://openssl-library.org/source/index.html) or later (or 1.1.1b).
- [BSV 1.2.0](https://github.com/bitcoin-sv/bitcoin-sv/releases) exact version.
- Please make sure MSBuild is in the PATH.
**Linux**: static libraries and must be compiled with `fPIC` on.
- C++ 20 compatible compiler
**Windows**: Visual Studio Community Edition 2022 on windows
**Linux**: g++13 on Linux
**MacOS**: clang 15 on Mac OS
##### Java module
- Java JDK 8 or later
- Download testng jar files `guice-4.1.0.jar`, `jcommander-1.72.jar`, `snakeyaml-1.21.jar` and `testng-7.1.0.jar`
- The `JAVA_TOOLS` environment variable must be setup pointing to the directory containing these files
##### Python module
- No additional requirements if Python is installed with Debug symbols

### Dependencies
Dependencies marked optional apply if you wish to run the unit tests. See [Tests](#Tests) for more details.

### Environment Variables

- Make sure the CMake bin directory is included in the `PATH` environment variable
- Make sure the Python3 directory is included in the `PATH` environment variable
- Make sure the `OPENSSL_ROOT_DIR` environment variable is set to the location where OpenSSL is installed
- Make sure the `BOOST_ROOT` environment variable is set to the location where boost is installed

##### Java module

- Make sure the `JAVA_TOOLS` environment variable is set to the location of `junit4.jar` and `hamcreast.jar`
By default all languange binding modules are built. If users don't want to build java module, add `-SKIP_MODULE_JAVA=ON` to cmake command, it will deactivate build of the java binding.

## Building Script Engine SDK
It is recommended that a build directory **build** is created outside of Script Engine SDK source code directory. See [directories structure](directories.md).

##### Windows

From the build directory:

To build Script Engine SDK
```console
cmake -G"Visual Studio 17 2022" -A x64 ..\bscrypt && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release
```

To run tests
```console 
ctest -C Release
```
or
```console 
ctest -C Debug
```

To create a Windows installer for Script Engine SDK you will need to install [NSIS 3.04](https://nsis.sourceforge.io/Download), then
```console 
cpack -G NSIS -C Release
```

##### Linux

From the build directory:

To build Script Engine SDK
```console
cmake ../bscrypt -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8
```
If you want to build in debug mode, add the flag `-DCMAKE_BUILD_TYPE=Debug` to the cmake command.

To run tests
```console
ctest
```

To create a Linux distribution
```console
cpack -G TGZ
```

## Tests
Once the build tools and libraries are prepared, some post installation steps are required to let the Script Engine SDK build system know how to find everything:

##### C++ on Linux
```console
make test
ctest
```

##### Java test from IntelliJ IDEA

In general to run script engine java test from any IDE, users need to let the IDE know where to load the sesdk.jar package and where is the location of the sesdk_jni runtime library. Below is the explanation of how to do it with IntelliJ IDEA.

Ctrl-Shift-A then type "Import project from existing source". Select directory $BSCRYPT, click next next a few time until reaching

  - The window asking to select some of the directories, uncheck all, leave only the $BSCRYPT/test/java
  - The window asking to select a library (jar file), uncheck all

When the project is created, IntelliJ open a new window with projec tree there. Open File/Project Structure, then go to Modules :

  - On Sources tab, select directory $BSCRYPT/test/java and make sure it is marked as "Test"
  - On Dependencies tab :

      - click on '+' button to add sesdk jar file ($BUILD_DIR/generated/tools/bin/sesdk-x.y.z.jar)
      - click on '+' button to add directory $JAVA_TOOLS
    Make sure the 2 dependencies added are checked to be used

Right click on a specific *Test.java file, select recompile
Right click on the same *Test.java file, select "create test" (there are symbol indicating TestNG), then the test configuration will open
From the VM options, add appropriate value of
```
-Djava_test_data_dir="$BSCRYPT/test/java/data" -Djava.library.path="$CMAKE_BUILD_DIR/x64/release"
```
Then the test is ready to run/debug. To debug java code, and jni C++ call:

- Make sure you've built in debug mode
- Change VM option `-Djava.library.path="$CMAKE_BUILD_DIR/x64/debug` 
- add the argument "-Ddebug=1" to VM options
- use the method "PackageInfo.getPID()" to get the pid of the proccess and use C++ debugger "attach to process" functionality.
