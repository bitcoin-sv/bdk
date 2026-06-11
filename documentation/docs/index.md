## Getting started

The Bitcoin Development Kit (BDK) provides facilities to work with Bitcoin SV scripts and
transactions from different languages. Its **core** is a C++ library (`bdk_core`) built from a
curated subset of the Bitcoin SV sources, and additional **modules** add features — notably language
bindings (e.g. the Go binding under `module/gobdk`). Users can extend the functionality by writing
their own modules.

- [Directory Structure](directories.md)
- [Development build](build.md)
- [Architecture overview](architecture.md)
- [Rust Binding](rust_binding.md)
- [VerifyScript](verify_script.md)
- [Versioning](versioning.md)
- [Object Model](ObjectModel.md)

---

## Install layout

After installing/unpacking a BDK package, the layout looks like this (rooted at the install prefix):

```
|-- include
|       |-- <bsv headers>          # bitcoin-sv headers, kept in their src/ subdirectory structure
|       |                          #   (e.g. crypto/, script/, primitives/, consensus/, ...)
|       |-- config                 # generated bitcoin config header
|       |-- core                   # extra BDK core headers + the single-include umbrella header `bdk`
|       |-- secp256k1
|       |       |-- include        # secp256k1 public headers
|       |-- univalue               # univalue public headers
|-- lib                            # static/shared libraries: bdk_core, secp256k1, univalue
|-- Documentation
|       |-- core_doc               # this documentation, built as HTML
```

- `include/core/BDKVersion.h` declares the version symbols recording how and when the package was
  built (values generated at build time; see [Versioning](versioning.md)).
- `lib/` contains the static (and any shared) libraries.
- `Documentation/core_doc/` contains the HTML documentation.

(These paths are taken from the install rules in `core/CMakeLists.txt`, `core/setting-secp256k1.cmake`
and `core/setting-univalue.cmake`, and the components packaged by `cmake/BDKCPackConfig.cpack.in`.)

## Usage

BDK is a multi-language library; it supports `C++` and `Golang`.

### C++

To build a C++ program against an installed BDK, add these include directories:

- `/path/to/bdk_install/include`
- `/path/to/bdk_install/include/core`
- `/path/to/bdk_install/include/secp256k1/include`
- `/path/to/bdk_install/include/univalue`

and link against the libraries in `/path/to/bdk_install/lib`.

For convenience there is a single umbrella header (installed at `include/core/bdk`):

```c++
#include <bdk>
```

This pulls in every header delivered by the package. It is simple but not optimal for compilation
time.

### Golang

See [Consuming the GoBDK module (cgo)](build.md#consuming-the-gobdk-module-cgo) for the cgo
environment setup.

### Viewing the documentation

The docs are delivered as HTML. To view them:

```console
python -m http.server -d /path/to/bdk_install/Documentation/core_doc
```

then open `http://localhost:8000` in a browser.
