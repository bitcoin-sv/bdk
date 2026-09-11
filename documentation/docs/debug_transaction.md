# Debugging transaction validation

Replay a failing transaction through the native C++ `example_txvalidator` program
to step into the validation engine. The Go/Rust/WASM adapters can obscure the
C++ call stack, and prebuilt archives may not contain useful debug symbols.
A native Debug build makes the engine's inputs, intermediate checks and typed
errors inspectable. Reproduce the caller's context before interpreting a failure.

## Gather the transaction and its context

Prefer the exact extended transaction from the failing caller. It contains the
ordinary transaction plus each input's previous output amount and locking script.
It does **not** contain network selection, UTXO creation heights, validation height,
consensus mode or custom policy settings. Collect network, validation-time chain
tip or containing-block height, mode and policy/activation overrides from the
original caller's request, configuration and logs. Collect UTXO creation heights
from its UTXO metadata or the parent transactions' block metadata. The fetcher
below obtains parent heights and the transaction's mined height separately from
the explorer API; those are not decoded from the hex. Explorer metadata must be
checked against the original validation-time context, especially for mempool replays.
A standard transaction's hex alone is insufficient for this example.

If only the transaction ID is available, use `woc tx` to retrieve the transaction
and enrich its inputs from their parent transactions. Run from the BDK root:

```bash
mkdir -p ../build-tools
(cd module/gobdk && go build -buildvcs=false -mod=mod -o ../../../build-tools/woc ./cmd/woc)
../build-tools/woc tx --help
export BDK_TXID=654cf5a35962eb2f2404666187b0f9759e802b0c54e1a7ed05efd09e7d041423
export BDK_NETWORK=main
export BDK_CASE_DIR="$(mktemp -d "$(cd .. && pwd)/bdk-tx-case.XXXXXX")"
../build-tools/woc tx --network "$BDK_NETWORK" --txID "$BDK_TXID" \
  --csv-file "$BDK_CASE_DIR/tx.csv"
```

`-buildvcs=false` disables VCS stamping, allowing this build from an exported
source tree without usable Git metadata.

The transaction ID above is the tool's documented example; replace it and the
network with the case being investigated. The parent directory must exist and
the CSV file must not exist. The tool refuses to overwrite it. `--txID` is
case-sensitive. Omit `--standard`: that flag requests ordinary hex and is
incompatible with CSV export.

Without `--csv-file`, the tool prints labelled extended hex and UTXO heights,
not a CSV record. Do not redirect that output into a `.csv` file. Successful CSV
export prints `Wrote 1 record to ...`; confirm the file contains a header and one
complete data record. API failure, missing parents or an unavailable transaction
must be resolved before replay. Do not substitute an empty record. If the service
cannot retrieve the case, obtain the original extended bytes and context from the
caller; the native replay itself does not require an explorer connection.

## CSV format and validation context

Put the supported gathered data into the CSV and pass its path with `--csv-file`.
The C++ example parses that file and reads the record data it needs. It skips the
first line and then requires exactly five columns in this order:

```text
ChainNet,BlockHeight,TXID,TxHexExtended, UTXOHeights
```

| Column | Required value |
|--------|----------------|
| `ChainNet` | Network name matching the executable's `--network` argument; use a separate run for each network. |
| `BlockHeight` | For consensus/block replay, the height of the block containing the transaction. For policy/mempool replay, the chain tip at the time of validation. |
| `TXID` | Ordinary transaction ID; the example recomputes it from the decoded transaction and compares it. |
| `TxHexExtended` | Complete extended transaction hex, including previous output data for every input. |
| `UTXOHeights` | One signed 32-bit creation height per input, in input order, separated by `|`, e.g. `620000|620001`. |

Use one transaction per case file while debugging. Keep all five fields nonempty,
include the header, and do not add blank lines. For manually supplied extended
bytes, create a CSV using this template, replacing every angle-bracket placeholder
before running it:

```text
ChainNet,BlockHeight,TXID,TxHexExtended, UTXOHeights
<network>,<validation-height>,<ordinary-txid>,<complete-extended-hex>,<input-0-height>|<input-1-height>
```

For one input, supply one height with no `|`. The number of heights must equal
the number of inputs. Ordinary hex must first be enriched with the previous
output amount and locking script for each input. The example does not perform
that enrichment, and the missing heights cannot be recovered from extended hex.

The CSV export uses the transaction's **mined height reported by the API**, or
zero when no mined height is reported. That is not automatically the desired
validation height. For a mempool replay, replace `BlockHeight` with the original
caller's chain tip. For an unconfirmed transaction, do not treat zero as a valid
reconstructed historical context. If the original tip is unknown, label the replay
as an experiment at a chosen tip, not an exact reproduction.

The fetcher also copies parent block heights as reported by the API. For an input
whose parent was unconfirmed at the time of a policy replay, BDK uses
`MEMPOOL_HEIGHT = 2147483647`, not zero. Replace only those input heights whose
unconfirmed status is established from the original context. Do not blindly map
all API zeroes to the sentinel. In a block replay, supply the actual creation
height, including the containing block's height for a parent earlier in the same
block. BDK rejects the mempool sentinel in consensus mode.

The existing `ChainNet` column records each row's network, but the validator's
network is selected by the process-wide `--network` argument. Every row must match
that argument. Do not add another network column: the parser still requires
exactly the five fields above.

Consensus mode is **not a CSV column**. The executable defaults to consensus mode;
`--disable-consensus` selects policy mode for every row in the file. Do not use
`--consensus`, `--consensus=false`, or a sixth CSV column.

Record the original BDK/BSV commits, network, failing API, policy and activation
height overrides, chain tip/block height, input heights, expected result, and
actual error alongside the CSV in a case note. The example creates a validator
with network defaults and has no CLI for policy overrides. If the caller changed
those defaults, the unmodified example alone is not an exact reproduction.
Use the debugger to inspect the validator and reproduce an override only through
its matching public setter with the captured value. If a value or matching setter
is unavailable, report the context gap rather than inventing a default. A source
change to add a richer harness is a separate task.

## Build the native debug executable

Use the [native prerequisites](build.md#prerequisites), including `BSV_ROOT`,
`BOOST_ROOT` and `OPENSSL_ROOT_DIR`. From the BDK root:

```bash
cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DBSV_ROOT="$BSV_ROOT" -DBOOST_ROOT="$BOOST_ROOT" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
  -DBUILD_MODULE_GOLANG=OFF -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=OFF \
  -DBUILD_MODULE_RUST=OFF -DBUILD_MODULE_RUST_INSTALL_INSOURCE=OFF
cmake --build ../build-debug --target example_txvalidator --parallel 4
export BDK_DEBUG_EXE="$(cd ../build-debug/x64/debug && pwd)/example_txvalidatord"
"$BDK_DEBUG_EXE" --help
```

The target name is `example_txvalidator`; the Debug executable has a `d` suffix.
The 64-bit output layout is `x64/debug`, not `module/example`. The Unix Debug
configuration uses `-O0 -g3`. Build the native target, not a prebuilt language
archive or the standalone WASM target.

## Replay the case

For a block/consensus case:

```bash
"$BDK_DEBUG_EXE" --network "$BDK_NETWORK" --csv-file "$BDK_CASE_DIR/tx.csv"
```

For a policy/mempool case, after setting the CSV's validation height correctly:

```bash
"$BDK_DEBUG_EXE" --network "$BDK_NETWORK" --csv-file "$BDK_CASE_DIR/tx.csv" --disable-consensus
```

The program reports row errors but can still return exit code zero. For a known
valid one-row smoke case, require `End Of Program, total csv 1 lines`, `Nb Txs 1`,
and no `ERROR` text. A reported `Nb Txs 0` is not a successful validation. For the
actual failing case, preserve the failure and inspect it; do not change consensus
mode merely to make the transaction pass.

## Step through the engine

On Linux, start GDB with the same arguments as the failing replay:

```bash
gdb --args "$BDK_DEBUG_EXE" --network "$BDK_NETWORK" --csv-file "$BDK_CASE_DIR/tx.csv"
# Append --disable-consensus above only for a policy replay.
```

At the GDB prompt:

```text
set pagination off
break doValidateTransaction
break bsv::CTxValidator::ValidateTransaction
break bsv::CTxValidator::implVerifyScript
catch throw
run
```

At `doValidateTransaction`, inspect `record.blockHeight`,
`record.dataUTXOHeights`, `record.txID` and `consensus`. Step through deserialization
and the transaction-ID check. At `ValidateTransaction`, use `next` to follow the
transaction checks and `step` to enter the first failing helper. `implVerifyScript`
is reached only if earlier checks succeed. There, inspect `index`, `utxoHeight`,
`flags`, `amount`, `lscript` and `uscript` after their declarations have executed.
Step through `bsvVerifyScript` into upstream `VerifyScript`/`EvalScript` when the
failure is in script execution.

To inspect the typed result before the example replaces it with a generic error,
set a breakpoint at the source line containing
`if (!bsv::TxErrorIsOk(ret))` in `module/example/example_txvalidator.cpp` (line 64
at the documented revision), continue to it, then run:

```text
print ret.domain
print ret.code
bt
```

Domain 0 is success; 1 is a script error; 2 is a transaction-validation/DoS error;
3 is an exception or cancellation result. Decode a script code using the pinned
bitcoin-sv `src/script/script_error.h`; decode a DoS code using BDK's
`core/doserror.hpp`. A domain named “DoS” does not mean this standalone program bans
peers. `catch throw` can reveal the original exception before the API converts it
to domain 3. Some caught exceptions may be incidental; use the stack and final
result to identify the relevant one.

If a breakpoint cannot be resolved, check that the executable is the fresh Debug
binary and that GDB can locate its source paths. A resolved breakpoint that is
never reached is a different condition: use `catch throw` and breakpoints on the
round-trip comparison (`record.txBinExtended != outBin`) and transaction-ID
comparison (`record.txID != recovTxID`) to detect failures before
`ValidateTransaction`. A throw during decoding or a failed comparison means the
input fixture has not passed the example's pre-validation checks; `ret` is not yet
available. Do not diagnose that as a missing-symbol failure or infer success from
a breakpoint that was never reached. On macOS, LLDB equivalents are
`lldb -- <executable> <arguments>`, `breakpoint set --name doValidateTransaction`,
`breakpoint set --name bsv::CTxValidator::ValidateTransaction`, `run`, `next`,
`step`, `frame variable`, and `thread backtrace`.

Keep a case report containing the exact command, CSV, source revisions, first
failing helper or input, typed error, relevant flags and context differences.
Native replay checks the supplied transaction and metadata; it does not establish
that those outputs were actually unspent in the original node's chain state.
