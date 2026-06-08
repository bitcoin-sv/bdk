# Bitcoin Development Kit (BDK)

BDK packages the Bitcoin SV (BSV) script and transaction-validation engine into a stand-alone C++
library (`bdk_core`) plus language bindings, so applications can validate Bitcoin SV transactions
with behaviour matching the node — without building the whole node. It is assembled at build time
from a curated subset of the `bitcoin-sv` sources and ships a Go (cgo) binding under
`module/gobdk`.

## Documentation

The full documentation is an mkdocs site under `documentation/docs/`:

- [About / getting started](documentation/docs/index.md)
- [Directory structure](documentation/docs/directories.md)
- [Development build](documentation/docs/build.md) — prerequisites, pinned dependency versions, and
  build/test commands
- [Architecture overview](documentation/docs/architecture.md)
- [VerifyScript](documentation/docs/verify_script.md) — consensus vs. policy validation paths
- [Versioning](documentation/docs/versioning.md)
- [Object model](documentation/docs/ObjectModel.md)

Please refer to the [build documentation](documentation/docs/build.md) for prerequisites and the
authoritative, CI-pinned dependency versions before building.
