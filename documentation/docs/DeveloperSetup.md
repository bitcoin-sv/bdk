## Getting the source code
Building bscrypt requires the source for both bscrypt and bsv repositories. 

For nChain developers, the best way to do this is to have a BitBucket account setup with [ssh key access](https://confluence.atlassian.com/bitbucket/set-up-an-ssh-key-728138079.html). If other access is available, the source code should be organised as per the directory structure outlined in [Architecture](Architecture.md)

## Prerequisites
##### General
- [Python 3.8 (64-bit)](https://www.python.org/downloads/release/python-380/) or later
**Windows**: If using the executable installer, ensure the "Download debugging symbols" and the "Download debug binaries" installation options are selected. 
- The following python packages are needed to install and build documentation and to run tests.
```console
python -m pip install pytest junitparser mkdocs pymdown-extensions plantuml_markdown
```
- [CMake 3.16](https://cmake.org/download/) or later
- [Boost 1.72](https://www.boost.org/doc/libs/1_72_0/) or later. 
**Linux**: static libraries and must be compiled with `fPIC` on.
- OpenSSL 1.1.1b or later
- C++ 17 compatible compiler
**Windows**: Visual Studio Community Edition 2019 on windows
**Linux**: g++9 on Linux
##### Java module
- Java JDK 8 or later
- `junit4.jar` and `hamcrest.jar`. (Rename if necessary)
- The `JAVA_TOOLS` environment variable must be setup to point to junit4.jar and hamcrest.jar
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

## Building bscrypt
It is recommended that a build directory **build** is created outside of bscrypt source code directory. See [Architecture](Architecture.md).

##### Windows

From the build directory:

To build bscrypt
```console
cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release
```

To run tests
```console 
ctest -C Release
```
or
```console 
ctest -C Debug
```

To create a Windows installer for bscrypt you will need to install [NSIS 3.04](https://nsis.sourceforge.io/Download), then
```console 
cpack -G NSIS -C Release
```

##### Linux

From the build directory:

To build bscrypt
```console
cmake ../bscrypt -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8
```

To run tests
```console
ctest
```

To create a Linux distribution
```console
cpack -G TGZ
```

## Tests
Once the build tools and libraries are prepared, some post installation steps are required to let the bscrypt build system know how to find everything:

##### Linux
```console
make test
ctest
```

