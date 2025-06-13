## Getting started

Bitcoin Development Kit provides a facilities to work with Bitcoin SV scripts with different languages. It has the core part building all fundamental script's functionalities as a C++ library and different modules imlementing extras features. Those modules can be language binding (Java, Python) allowing developers from these programming background to develop scripts. Users are able to extend the functionalities by implementing their own modules.

- [Directories Structure](directories.md)
- [Development build](build.md)
- [VerifyScript](verify_script.md)
- [Object Model](ObjectModel.md)

---

After unpacking the Bitcoin Development Kit installer to the local machine, the content of the installation directory looks as below

```
|-- bdk_install
|       |-- include
|              |-- bitcoin-sv header files
|              |-- core
|              |-- secp256k1
|              |-- univalue
|       |-- lib
|       |-- Documentation
|               |-- core_doc
```

- File `include/core/BDKVersion.h` contains full version's information of how and when the installer was built.
- `include` directory contains all `*.h` and `*.hpp` files from bsv source code.
- `include/core` directory contains all extra `*.h` and `*.hpp` files declaring additional functionalities in Bitcoin Development Kit core.
- lib directory contains all archives (static) and runtime (shared) libraries.
- Documentation directory contains html documents.

## Usages
Bitcoin Development Kit is a multi languages library, it allows users to work with `C++`, `Java` and `Python`.

#### C++
To build a C++ program using Bitcoin Development Kit, it needs to link with the installed Bitcoin Development Kit:

- Let compiler know additional include directories are
    - `/path/to/bdk_install/include`
    - `/path/to/bdk_install/include/secp256k1`
    - `/path/to/bdk_install/include/univalue`
    - `/path/to/bdk_install/include/core`
- Let the compiler know additional library directory is `/path/to/bdk_install/lib`

To simplify for C++ code there are only one single file to include
```c++
#include <bdk>
```
This will include all header files delivered by the package. Note that it is simplified, but not optimal for compilation time.


#### Java
Java library `.jar` was build with `JNI`, it depend to a shared library, which is delivered in the same directory. To be able to correctly load the `.jar` library, it is required to set in the IDE

- _java.library.path_=`/path/to/bdk_install/lib`

#### Python
Similar to Java, python module need to load the shared library interface. In order to allow python loading the binding module, it require to set _PYTHONPATH_ environment pointing to `/path/to/bdk_install/lib`.


#### Documentation
Documentations are build and delivered as html contents. To visualize it:
```
python -m http.server -d /path/to/bdk_install/Documentation/core_doc
```
Then use a web browser to open the address `localhost:8000`
