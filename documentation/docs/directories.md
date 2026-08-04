# Directory Structure

The recommended layout places a `bdk` checkout and a `bitcoin-sv` checkout side by side, with an
out-of-tree `build` directory:

```
<workspace>
|-- build                 # out-of-tree build directory (created by you)
|-- bitcoin-sv            # external BSV checkout, pinned to the CI commit (see build.md)
|-- bdk
     |-- core             # the BDK C++ library (bdk_core) + curated BSV-subset sources
     |-- module           # language-binding / extension modules (link bdk_core; examples & gobdk also compile the BSV "application" sources)
     |    |-- gobdk       # Go (cgo) binding: github.com/bitcoin-sv/bdk/module/gobdk
     |    |-- example     # C++ examples & benchmarks
     |    |-- typesbdk    # opt-in, validated WASM transaction-script verifier
     |-- test
     |    |-- core        # C++ (ctest) tests
     |    |-- golang      # Go tests
     |    |-- rust        # Rust tests
     |    |-- types       # typesbdk WASM tests (node scripts + every wasm CTest registration)
     |-- cmake            # CMake helpers, find-modules, doc tooling, packaging config
     |-- documentation    # mkdocs site (this documentation) + core_doc build target
```

- **`bitcoin-sv`** is an external checkout of the SV codebase. BDK compiles a curated subset of its
  sources rather than vendoring them; see [Architecture overview](architecture.md) and
  [How BDK finds the bitcoin-sv source](build.md#how-bdk-finds-the-bitcoin-sv-source).
- **`core`** contains the files required to build BDK as a stand-alone C++ component (`bdk_core`)
  from the SV codebase, plus BDK's own sources (`txvalidator`, `assembler`, `chainparams_bdk`,
  `doserror`, `extendedTx`, `txerror`, `validatearg`, and the generated `BDKVersion`).
- **`module`** (note: **singular**) contains language wrappers / extension applications for the
  BDK component. The design intent is that modules build on `core` and stay independent of each
  other. In practice they link `bdk_core`, and the C++ examples and the GoBDK cgo library also
  compile in a small set of additional BSV "application" sources directly (see
  [Architecture overview](architecture.md#the-curated-bitcoin-sv-source-subset)).

#### Adding functionality to core

To add functionality to core, drop `*.h`/`*.hpp` and `*.cpp` files into the `core/` directory.
CMake globs them in automatically (`core/CMakeLists.txt`). Don't forget to add a corresponding test
under `test/core`.
