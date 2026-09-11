## Getting started

The Bitcoin Development Kit (BDK) provides facilities to work with Bitcoin SV scripts and
transactions from different languages. Its **core** is a C++ library (`bdk_core`) built from a
curated subset of the Bitcoin SV sources, and additional **modules** add features — notably language
bindings (e.g. the Go binding under `module/gobdk`). Users can extend the functionality by writing
their own modules.

- [Architecture overview](architecture.md)
- [Directory Structure](directories.md)
- [Development Build](build.md)
- [Debugging transaction validation](debug_transaction.md)
- [VerifyScript](verify_script.md)
- [Versioning](versioning.md)
- [Object Model](ObjectModel.md)

---

## Install layout

The native install rules define the following layout under the install prefix. Optional content depends on the enabled build and install targets; inspect a packaged archive separately before assuming it contains the same components:

```
|-- include
|       |-- <bsv headers>          # bitcoin-sv headers, kept in their src/ subdirectory structure
|       |                          #   (e.g. crypto/, script/, primitives/, consensus/, ...)
|       |-- config                 # generated bitcoin config header
|       |-- <BDK headers>          # BDK .h/.hpp files currently installed directly under include/
|       |-- core                   # generated umbrella header `bdk`
|       |-- secp256k1
|       |       |-- include        # secp256k1 public headers
|       |-- univalue               # univalue public headers
|-- lib                            # static/shared libraries: bdk_core, secp256k1, univalue
|-- Documentation
|       |-- core_doc               # this documentation, built as HTML
```

- `include/BDKVersion.h` declares the version symbols recording how and when the package was
  built (values generated at build time; see [Versioning](versioning.md)).
- `lib/` contains the static (and any shared) libraries.
- `Documentation/core_doc/` contains the HTML documentation.

These paths come from the install rules in `core/CMakeLists.txt`,
`core/setting-secp256k1.cmake`, `core/setting-univalue.cmake` and
`documentation/CMakeLists.txt`. CPack component selection is defined separately in
`cmake/BDKCPackConfig.cpack.in`.

## Usage

BDK provides a C++ core, a Go (cgo) binding, a TypeScript/JavaScript binding through WebAssembly, and an experimental Rust binding. Their APIs differ; consult each module's documentation for the operations it exposes.

### C++

The C++ API is implemented by `bdk_core`; see [Object Model](ObjectModel.md).
Build and run the repository's C++ examples using the
[Development Build](build.md) instructions. The
[transaction debugging guide](debug_transaction.md) uses `example_txvalidator`.

An installed package provides headers under `include/`, `include/core/`,
`include/secp256k1/include/` and `include/univalue/`, with archives under `lib/`.
The generated umbrella header is installed as `include/core/bdk`.

The installed umbrella is not currently a usable consumer shortcut: installed
`serialize.h` requires `compat/endian.h`, which is missing from the install. A
syntax-only `#include <bdk>` check fails with `fatal error: compat/endian.h: No such
file or directory`. The install rules also put BDK's extra headers directly under
`include/`, while the umbrella refers to them under `core/`. The source-tree
examples remain the reference for building native consumers.

### Golang

See [Consuming the GoBDK module (cgo)](build.md#consuming-the-gobdk-module-cgo) for the cgo
environment setup.

### TypeScript / JavaScript (WebAssembly)

`module/typesbdk/wasm` supplies WebAssembly binaries and JavaScript loaders for
Node.js and browsers. See the
[WASM module layout](https://github.com/bitcoin-sv/bdk/blob/master/module/typesbdk/wasm/README.md),
[API examples](https://github.com/bitcoin-sv/bdk/blob/master/module/typesbdk/examples/README.md),
and [standalone WASM build](build.md#the-standalone-wasm-build).
The committed artifacts are refreshed on demand and may lag the source.

### Rust (experimental)

The experimental binding lives in `module/rustbdk`. Its safe package is `rust-bdk`,
imported as `rust_bdk`; `bdk-sys` owns the raw FFI. Build the `MergeBDKFFI` target
before compiling Rust consumers, or explicitly provide a compatible archive via
`BDK_LIB_DIR`. Cargo does not download the archive automatically.
See [building the Rust binding](build.md#building-the-rust-binding-experimental),
the [Rust README](https://github.com/bitcoin-sv/bdk/blob/master/module/rustbdk/README.md),
and [module independence rules](architecture.md#module-independence-rules).

### Viewing the documentation

The docs are delivered as HTML. To view them:

```console
python -m http.server -d /path/to/bdk_install/Documentation/core_doc
```

then open `http://localhost:8000` in a browser.
