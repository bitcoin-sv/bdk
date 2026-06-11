# Rust BDK

`module/rustbdk` is an experimental Rust binding for BDK transaction validation.
It wraps `bdk_core` through rustbdk's own `bdkffi_` C ABI and exposes the safe
crate `rust-bdk` (imported as `rust_bdk` in Rust code).

This is not the similarly named Bitcoin Dev Kit project. If this crate is ever
published outside this repository, it should use a less ambiguous package name.

## Build precondition

`rust-bdk` links a local CMake-built static archive. **A CMake build of
`BUILD_MODULE_RUST` must run before `cargo build`, `cargo test`, or
`cargo bench`.** Source-build is the only v1 mode; rustbdk does not commit or
download prebuilt `libbdkffi` archives yet.

From the BDK repository root:

```console
cmake -B build -S . -DBUILD_MODULE_RUST=ON -DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON
cmake --build build --target MergeBDKFFI
cargo build --manifest-path module/rustbdk/Cargo.toml -p rust-bdk
```

## Install

For an in-repository consumer, depend on the safe crate by path:

```toml
[dependencies]
rust-bdk = { path = "module/rustbdk/rust-bdk" }
```

For a consumer in another repository, use a BDK git checkout and point Cargo at
that checkout:

```console
git clone https://github.com/bitcoin-sv/bdk.git
cd bdk
cmake -B build -S . -DBUILD_MODULE_RUST=ON -DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON
cmake --build build --target MergeBDKFFI
```

Then in the consuming crate:

```toml
[dependencies]
rust-bdk = { path = "/path/to/bdk/module/rustbdk/rust-bdk" }
```

Direct Cargo `git = "..."` consumption is deferred until the repository has a
root Rust manifest or published rustbdk archives.

## Minimal Usage

```rust
use rust_bdk::{TxError, TxValidator};

fn validate(extended_tx: &[u8], utxo_heights: &[i32], block_height: i32) -> Result<(), TxError> {
    let validator = TxValidator::new("main").expect("known BDK network");

    // consensus=true means block-validation rules. Use false for peer/mempool
    // policy validation.
    validator.validate_transaction(extended_tx, utxo_heights, block_height, true)
}
```

`extended_tx` must be BDK's extended transaction byte format, and
`utxo_heights` must contain one height per input.

## Supported Platforms

The v1 Rust binding follows the BDK module platform set:

- Linux x86_64
- Linux aarch64
- macOS arm64
- macOS x86_64 is supported by the archive naming/link logic, but is not covered
  by CI yet

Windows is not supported.

## Link Overrides

`bdk-sys/build.rs` searches for `libbdkffi_<os_arch>.a` in the local
`bdk-sys/lib/` output, CMake install output, and common build-tree locations.
These environment variables override the defaults:

- `BDK_LIB_DIR`: directory containing the rustbdk archive.
- `BDK_LIB_NAME`: archive stem without `lib` or `.a`, for example
  `bdkffi_linux_x86_64`.
- `BDK_CXX_RUNTIME`: C++ runtime to link, one of `stdc++`, `c++`, or `none`.
  Defaults are `stdc++` on Linux and `c++` on macOS.

`BDK_LIB_NAME` is only a library stem, not a path, and must still refer to a
rustbdk `libbdkffi` archive.

## Benchmarks

The production archive is clean by default: benchmark-only `bdkffi_bench_*`
symbols are not compiled unless `BDKFFI_ENABLE_BENCH=ON` is set during the CMake
build. The safe benchmark helper module is also behind the Rust `bench` feature.
Both switches are required because CMake owns the static archive contents while
Cargo owns the Rust benchmark targets; the two build systems cannot enforce this
coupling mechanically.

Criterion benches live under `rust-bdk/benches/`:

- `bench_ffi_noop`: safe wrapper over a no-op `bdkffi_` call.
- `bench_sum_bytes_1k`, `bench_sum_bytes_10k`, `bench_sum_bytes_100k`: pointer
  and length marshalling with tracked in-source vectors.
- `bench_validate_transaction_single`: single safe validation call.
- `bench_validate_transaction_batch`: prebuilt owning batch validation.
- `bench_batch_add`: owning `ValidateBatch::add` cost, with a Rust vector-copy
  baseline. There is no constructible zero-copy safe batch in v1.

Run them only after building the archive with benchmark helpers enabled:

```console
cmake -B build -S . -DBUILD_MODULE_RUST=ON -DBDKFFI_ENABLE_BENCH=ON -DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON
cmake --build build --target MergeBDKFFI
cargo bench --manifest-path module/rustbdk/Cargo.toml -p rust-bdk --features bench
```

Failure modes are intentional:

- If the Cargo `bench` feature is off, the `required-features` bench targets are
  skipped.
- If the Cargo `bench` feature is on but the archive was built with
  `BDKFFI_ENABLE_BENCH=OFF`, linking fails with undefined `bdkffi_bench_*`
  symbols.

Results are **to be measured**. Expected direction, not measured fact: the Rust
FFI no-op should be much cheaper than Go cgo's no-op transition, and transaction
validation should stay within noise of the C++ example because both call the
same `bdk_core` implementation. Any material gap should be investigated as
marshalling overhead or a benchmark setup issue.

## Independence

rustbdk is a sibling module, not a wrapper around GoBDK. It depends on
`bdk_core`, shared third-party/link-closure inputs, and its own `bdkffi_`
archive. It must not include, link, copy, read at build time, or edit
`module/gobdk`.
