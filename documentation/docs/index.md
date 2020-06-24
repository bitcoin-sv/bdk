## Getting started

Bscrypt provides a facilities to work with Bitcoin SV scripts with different languages. It has the core part building all fundamental script's functionalities as a C++ library and different modules imlementing extras features. Those modules can be language binding (Java, Python) allowing developers from these programming background to develop scripts. Users are able to extend the functionalities by implementing their own modules.

- [Architecture](Architecture.md)
- [Development setup](DeveloperSetup.md)
- [Object Model](ObjectModel.md)

---

After unpacking the bscrypt installer to the local machine, the content of the installation directory looks as below

```
|-- bscrypt_install
|       |-- include
|              |-- core
|              |-- ScriptEngineIF
|              |-- secp256k1
|              |-- univalue
|              |-- ScryptVerion.hpp
|       |-- lib
|       |-- Documentation
|               |-- core_doc
```

- include directory contains all `*.h` and `*.hpp` files.
- lib directory contains all archives (static libraries) and shared libraries using at run time.
- File `ScryptVersion.hpp` contains full version's information of how and when the installer was built.
- Documentation directory contains html documents.

## Documentation
Documentations are build and delivered as html contents. To visualize it:
```
python -m http.server -d /path/to/bscrypt_install/Documentation/core_doc
```
Then use a web browser to open the address localhost:8000

## Usages
BScrypt is a multi languages library, it allows users to work with `C++`, `Java` and `Python`.

#### C++
To build a C++ program using bscrypt, it needs to link with the installed bscrypt:
- Let compiler know additional include directories are
    - `/path/to/bscrypt_install/include/core`
    - `/path/to/bscrypt_install/include/ScriptEngineIF`
    - `/path/to/bscrypt_install/include/secp256k1`
    - `/path/to/bscrypt_install/include/univalue`
- Let the compiler know additional library directory is `/path/to/bscrypt_install/lib`

#### Java
Java library `.jar` was build with `JNI`, it depend to a shared library, which is delivered in the same directory. To be able to correctly load the `.jar` library, it is required to set in the IDE

- `java.library.path`=/path/to/bscrypt_install/lib

#### Python
Similar to Java, python module need to load the shared library interface. In order to allow python loading the binding module, it require to set `PYTHONPATH` environment pointing to `/path/to/bscrypt_install/lib`.
