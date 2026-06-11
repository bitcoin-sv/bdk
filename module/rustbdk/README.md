# Rust BDK

`module/rustbdk` is an experimental Rust binding for BDK transaction validation.
It wraps `bdk_core` through rustbdk's own `bdkffi_` C ABI and exposes the safe
crate `rust-bdk` (imported as `rust_bdk` in Rust code).

This is not the similarly named Bitcoin Dev Kit project. If this crate is ever
published outside this repository, it should use a less ambiguous package name.

## Quickstart: a Rust app from scratch

`rust-bdk` links a local CMake-built static archive. **A prior CMake build of
`BUILD_MODULE_RUST` is mandatory before any `cargo` command**, including
`cargo build`, `cargo run`, `cargo test`, and `cargo bench`. Source-build is the
only v1 mode; rustbdk does not commit or download prebuilt `libbdkffi` archives
yet.

For compiler, Boost, OpenSSL, and other BDK build prerequisites, use the main
[BDK build documentation](../../documentation/docs/build.md). The steps below
only cover the rustbdk-specific flow.

1. Clone BDK and build the Rust FFI archive:

```console
git clone https://github.com/bitcoin-sv/bdk.git && cd bdk
cmake -B build -S . -DBUILD_MODULE_RUST=ON -DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON
cmake --build build --target MergeBDKFFI
```

`-DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON` is required for this copy-paste flow
because it installs `libbdkffi` into `module/rustbdk/bdk-sys/lib/`, where
`bdk-sys/build.rs` finds it when an external app links `rust-bdk` by path. If
you do not install in-source, set `BDK_LIB_DIR` to the directory containing the
`libbdkffi_<os_arch>.a` archive before running Cargo.

2. Create a new Cargo app outside the BDK repository:

```console
cd ..
cargo new my-bdk-app && cd my-bdk-app
```

3. Add `rust-bdk` as a path dependency in `Cargo.toml`:

```toml
[dependencies]
rust-bdk = { path = "/absolute/path/to/bdk/module/rustbdk/rust-bdk" }
```

Use the absolute path to the BDK checkout you built in step 1. `rust-bdk` uses
Rust edition 2024, so use a recent Rust toolchain with edition 2024 support.

4. Replace `src/main.rs` with this complete program:

```rust
use rust_bdk::{
    TxValidator, bdk_rust_version_string, bsv_version_string, from_asm, to_asm,
};

fn main() {
    let network = "main";
    let validator = TxValidator::new(network).expect("known BDK network");

    let input_asm = "4 5 ADD 9 EQUAL";
    let script = from_asm(input_asm);
    let output_asm = to_asm(&script);
    assert_eq!(from_asm(&output_asm), script);

    println!("rust-bdk version: {}", bdk_rust_version_string());
    println!("bsv version: {}", bsv_version_string());
    println!("network: {network}");
    println!(
        "genesis activation height: {}",
        validator.genesis_activation_height()
    );
    println!("asm round-trip: {output_asm}");
}
```

5. Build and run the app:

```console
cargo run
```

Expected output, with version strings shown as illustrative placeholders:

```text
rust-bdk version: <rust-bdk-generated-version>
bsv version: <bsv-version>
network: main
genesis activation height: 620538
asm round-trip: 4 5 ADD 9 EQUAL
```

`rust-bdk`'s version string is generated independently from the linked BSV
version. The network, Genesis activation height, and asm round-trip lines are
deterministic for this program.

Even faster, after the archive is built in step 1, run the bundled validator
example from the BDK checkout:

```console
cd /absolute/path/to/bdk && cargo run --manifest-path module/rustbdk/Cargo.toml -p rust-bdk --example txvalidator
```

It prints `validation: ok` when validation succeeds.

For an in-repository consumer, depend on the safe crate by path:

```toml
[dependencies]
rust-bdk = { path = "module/rustbdk/rust-bdk" }
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
