## Getting the source code
Building Bitcoin Development Kit requires the source for both BDK and bsv repositories. 

If other access is available, the source code should be organised as per the directory structure outlined in [directories structure](directories.md)

## Prerequisites
##### General
- [Python 3.12 (64-bit)](https://www.python.org/downloads/release/python-3125) or later. For _windows_, if using the executable installer, ensure the "Download debugging symbols" and the "Download debug binaries" installation options are selected. 
- The following python packages are needed to install and build documentation and to run tests.
```console
python -m pip install pytest junitparser mkdocs pymdown-extensions plantuml_markdown
```
- C++ 20 compatible compiler
- [CMake 3.30.2](https://cmake.org/download/) or later
- [Boost 1.85](https://www.boost.org/doc/libs/1_85_0/) or later. On _Mac OS_, Boost 1.86 will break the compiler clang
- [OpenSSL 3.4.0](https://openssl-library.org/source/index.html) or later (or 1.1.1b).
- [BSV 1.2.0-Chronicle](https://github.com/bitcoin-sv/bitcoin-sv/releases) exact version.

Static libraries (boost and openssl) must be compiled with `fPIC` on.

- Windows : Visual Studio Community Edition 2022 on windows, make sure MSBuild is in the PATH.
- Linux : g++13 on Linux
- MacOS : clang 15 on Mac OS

Note that when building in Mac OS with clang, it only work with boost 1.85. As in boost 1.86, it seems the `boost::uuids::uuid::data_type` has changed, making the reinterpret_cast at `src/serialize.h:989` broken.

See [prebuild dependancies](https://github.com/bitcoin-sv/bdk/blob/master/.github/workflows/prebuild_dependancies.yaml) to better understand the tooling and versions. The CI build and test is using [github workflow](https://github.com/bitcoin-sv/bdk/blob/master/.github/workflows/build_bdk.yaml).

##### Java module
- Java JDK 11 or later
- Download testng jar files `guice-4.1.0.jar`, `jcommander-1.72.jar`, `snakeyaml-1.21.jar` and `testng-7.1.0.jar`
- The `JAVA_TOOLS` environment variable must be setup pointing to the directory containing these files

If these packages above is not dowloaded and setup with `JAVA_TOOLS`, cmake can manage on its own, but it'll require internet connection.

##### Python module
- No additional requirements if Python is installed with Debug symbols

##### Golang module
Golang binding built with CGO is working fine on Linux and Mac OS. For windows, it is not well tested, as the linking with DLL is not common.
The golang module is build into a shared library, then CGO will link dynamically to it. The golang modules builds and checkin the binaries files in git repo, so users only need to do `go get ...` to get all required to run.

The CGO binaries are stored in `module/gobdk/bdkcgo` for all supported system (Linux/MacOS with both arch amd/arm). These static libraries requires some system runtime libraries, mainly runtime C/C++. This means when building a `go` application with this package, the build might pass but the executable will not be able to run if these runtime libraries are missing from the system. To investigate what are the runtime dependancies of your executable, you can use `ldd` command (`otool` in MacOS). For example

```
cd module/gobdk && go build -o test_woc ./cmd/woc &&  ldd test_woc && cd ../..
        linux-vdso.so.1 (0x00007ffc709b7000)
        libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x0000780f65400000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x0000780f65746000)
        libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x0000780f65718000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x0000780f65000000)
        /lib64/ld-linux-x86-64.so.2 (0x0000780f65839000)
```
If you want to build and put your _go application_ in a _scratch_ container, then you'll need to copy over these shared libraries. Note that these shared libraries might require also other shared libraries ( not many though ), then use `ldd` again to find out what need to be copied over.

### Dependencies
Dependencies marked optional apply if you wish to run the unit tests. See [Tests](#tests) for more details.

### Environment Variables

- Make sure the CMake bin directory is included in the `PATH` environment variable
- Make sure the Python3 directory is included in the `PATH` environment variable
- Make sure the `OPENSSL_ROOT_DIR` environment variable is set to the location where OpenSSL is installed
- Make sure the `BOOST_ROOT` environment variable is set to the location where boost is installed

##### Java module

- Make sure the `JAVA_TOOLS` environment variable is set to the location of `junit4.jar` and `hamcreast.jar`
By default all languange binding modules are built. If users don't want to build java module, add `-BUILD_MODULE_JAVA=OFF` to cmake command, it will deactivate build of the java binding.

## Building Bitcoin Development Kit
It is recommended that a build directory **build** is created outside of Bitcoin Development Kit source code directory. See [directories structure](directories.md).

##### Windows

From the build directory:

To build Bitcoin Development Kit
```console
cmake -G"Visual Studio 17 2022" -A x64 ..\bdk && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release
```

To run tests
```console 
ctest -C Release
```
or
```console 
ctest -C Debug
```

To create a Windows installer for Bitcoin Development Kit you will need to install [NSIS 3.04](https://nsis.sourceforge.io/Download), then
```console 
cpack -G NSIS -C Release
```

##### Linux

From the build directory:

To build Bitcoin Development Kit
```console
cmake ../bdk && make -j8
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
Once the build tools and libraries are prepared, some post installation steps are required to let the Bitcoin Development Kit build system know how to find everything:

##### C++ on Linux
```console
make test
ctest
```

##### Java test from IntelliJ IDEA

In general to run BDK java test from any IDE, users need to let the IDE know where to load the bdk.jar package and where is the location of the bdk_jni runtime library. Below is the explanation of how to do it with IntelliJ IDEA.

Ctrl-Shift-A then type "Import project from existing source". Select directory $BDK, click next next a few time until reaching

  - The window asking to select some of the directories, uncheck all, leave only the $BDK/test/java
  - The window asking to select a library (jar file), uncheck all

When the project is created, IntelliJ open a new window with projec tree there. Open File/Project Structure, then go to Modules :

  - On Sources tab, select directory $BDK/test/java and make sure it is marked as "Test"
  - On Dependencies tab :

      - click on '+' button to add bdk jar file ($BUILD_DIR/generated/tools/bin/bdk-x.y.z.jar)
      - click on '+' button to add directory $JAVA_TOOLS
    Make sure the 2 dependencies added are checked to be used

Right click on a specific *Test.java file, select recompile
Right click on the same *Test.java file, select "create test" (there are symbol indicating TestNG), then the test configuration will open
From the VM options, add appropriate value of
```
-Djava_test_data_dir="$BDK/test/java/data" -Djava.library.path="$CMAKE_BUILD_DIR/x64/release"
```
Then the test is ready to run/debug. To debug java code, and jni C++ call:

- Make sure you've built in debug mode
- Change VM option `-Djava.library.path="$CMAKE_BUILD_DIR/x64/debug` 
- add the argument "-Ddebug=1" to VM options
- use the method "PackageInfo.getPID()" to get the pid of the proccess and use C++ debugger "attach to process" functionality.

##### Golang module enabling cgo

When a project use gobdk, it has to enable cgo, and set some environments variable to cgo know where to find the installed `GoBDK`. Assuming the `bdk` package was unpacked to a directory `BDK_INSTALL_ROOT`.

On Linux :
```
export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include ${CGO_CFLAGS}"
export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin ${CGO_LDFLAGS}"
export LD_LIBRARY_PATH="${BDK_INSTALL_ROOT}/bin:${LD_LIBRARY_PATH}"
```

On Mac OS :
```
export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include ${CGO_CFLAGS}"
export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin -Wl,-rpath,${BDK_INSTALL_ROOT}/bin ${CGO_LDFLAGS}"
```

##### Prepare environment to build and test on MacOS X (ARM)

Download and install CMake 3.30.5 (Make sure the correct version)
Either install by brew or download and install the prebuilt from https://cmake.org/download/

Prepare some directories

```
export DEV_DIR=$HOME/development/devtool
mkdir -p $DEV_DIR
export DOWNLOAD_DIR=$HOME/Downloads/src
mkdir -p $DOWNLOAD_DIR && cd $DOWNLOAD_DIR
```

Build OpenSSL 3.0.14 from Source (About 5 minutes)
```
export OPENSSL_ROOT_DIR=$DEV_DIR/openssl-3.0.14
brew install perl zlib
# If not Mac ARM64, adapt "darwin64-arm64-cc" appropriately 
cd $DOWNLOAD_DIR && wget https://www.openssl.org/source/openssl-3.0.14.tar.gz && tar -xvzf openssl-3.0.14.tar.gz && cd $DOWNLOAD_DIR/openssl-3.0.14 && ./Configure darwin64-arm64-cc -no-shared --prefix=$OPENSSL_ROOT_DIR --openssldir=$OPENSSL_ROOT_DIR && make -j8 && make install
```

Build BOOST 1.85.0 from Source (About 5 minutes)

```
export BOOST_ROOT=$DEV_DIR/boost_1_85_0
cd $DOWNLOAD_DIR && wget https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz && tar -xvzf boost-1.85.0-cmake.tar.gz && cd boost-1.85.0 && mkdir build && cd build && cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=$BOOST_ROOT && make -j8 && make install
```

Build and test bdk (About 5 minutes). It'll need to download the source of bitcoin-sv so we'll have the structure

```
     $DOWNLOAD_DIR 
          |- bitcoin-sv
          |- bdk
```

Then run build/test/install/pack the bdk

```
cd $DOWNLOAD_DIR # Download the source code of bsv 1.2.0 inside this directory and name it bitcoin-sv
cd $DOWNLOAD_DIR && git clone git@github.com:bitcoin-sv/bdk.git && cd bdk && mkdir build && cd build && cmake .. && make -j8 && ctest && make install && cpack -G TGZ
```