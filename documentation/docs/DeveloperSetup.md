In order to develop/ contribute to bscrypt, it is required to install and prepare some dev environments. It consists of installing tools/libraries for C++/Java/Python. It is recommended to:

- Build from source when using a Linux environment
- Download prebuilt binaries when using a Windows environment
- Install everything in the same location and define environment variables pointing to the installed location. The build system will find them appropriately

---

## Getting the source code
Working with bscrypt development requires having the source code of bscrypt and bsv repositories. The best way to do is to have your bitbucket account setup with [ssh key access](https://confluence.atlassian.com/bitbucket/set-up-an-ssh-key-728138079.html).

If you don't have access to bitbucket repositories, you can download the source code from bsv and bscrypt, then manually put them in the directory structure as explained in [Architecture](Architecture.md)

## Prerequisite Tools
- [Python 3.8 (64-bit)](https://www.python.org/downloads/release/python-380/) - 
As part of the Python Windows installer, ensure to check the installation options for the "Download debugging symbols" and the "Download debug binaries" checkboxes. There are some additional python package that need to be installed for building documentation and running test.
```console
C:\nchain> python -m pip install mkdocs pymdown-extensions plantuml_markdown junitparser
```
- [CMake 3.16](https://cmake.org/download/)
- [Boost 1.72](https://www.boost.org/doc/libs/1_72_0/). Make sure it has static linking libraries and all has been compiled with fPIC if Linux
- OpenSSL 1.1.1b
- C++17 Compiler: Visual Studio Community Edition 2019 on windows, g++9 on Linux
- Java JDK > 8
- Have access to bitcoin sv repository
---

## Dependencies
Dependencies marked optional apply if you wish to run the unit tests. See [Tests](#Tests) for more details.

## Build Environments

- Make sure the directory of CMake executable to the environment variable PATH
- Make sure the directory of Python3 executable to the environment variable PATH
- Make sure the environment variable OPENSSL_ROOT_DIR is set to the location where openssl is installed
- Make sure the environment variable BOOST_ROOT is set to the location where boost is installed
- Download or clone the sv source code beside the bscrypt source code (bsv and bscrypt same parent directory level)
- Create a build directory **build_dir** outside of bscrypt

---

**On Windows**

```console
C:\nchain\build_dir> cmake -G"Visual Studio 16 2019" -A x64 ..\bscrypt && cmake --build . --target ALL_BUILD --config Debug && cmake --build . --target ALL_BUILD --config Release
```

If you wish to execute our test framework
```console 
C:\nchain\build_dir> ctest -C Release
```

If you wish to create a Windows installer program you will need to install [NSIS 3.04](https://nsis.sourceforge.io/Download)
```console 
C:\nchain\build_dir> cpack -G NSIS -C Release
```

**On Linux**

```console
nchain@build_dir:~$ cmake ../bscrypt -DCMAKE_BUILD_TYPE=Debug -DCUSTOM_SYSTEM_OS_NAME=Ubuntu; time -p make -j8
```

If you wish to execute our test framework
```console
nchain@build_dir:~$ ctest
```

If you wish to create a Linux distribution
```console
nchain@build_dir:~$ cpack -G TGZ
```

---

## Tests <a name="Tests"></a>

Once the build tools and libraries are prepared, some post installation steps are required to let the bscrypt build system know how to find everything:

**To run all tests**

```console
nchain@build_dir:~$ ctest             # or "make test" on linux

C:\nchain\build_dir> ctest -C Release # On Windows
C:\nchain\build_dir> ctest -C Debug   # On Windows
```