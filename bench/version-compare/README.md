# gobdk version comparison benchmark

Times `script.TxValidator.ValidateTransaction` (plain, non-cancellable) on fixed
transaction shapes, built against several gobdk releases from one source.
Not part of the library; not for merge.

```
./build.sh v1.3.0 v1.3.1 v1.4.0     # bin/bdk-bench-<version>
ROUNDS=10 CORE=2 ./run.sh           # results/<version>.jsonl, results/<version>.bench.txt
benchstat results/v1.3.1.bench.txt results/v1.4.0.bench.txt
```

- One process per (version, shape, round); versions interleaved and rotated
  each round; each process is pinned to one core with `taskset` and
  `GOMAXPROCS=1`, and killed after `TIMEOUT` seconds (recorded as `timeout`,
  and that version/shape is skipped in later rounds).
- Every call's result is checked against the expected verdict; a wrong verdict
  fails the run.
- Shapes are deterministic and cached in `results/cache`, so every version
  validates identical bytes (`tx_sha256` in the JSON).
- All shapes validate in consensus mode at block height 972000 (after the
  Genesis and Chronicle activation heights), except the `_policy` ones.
- On x86_64, `run.sh` adds a pass of v1.4.0 with `BDK_SHA256_FORCE_SCALAR=1`.

| Shape | What it is |
|---|---|
| `p2pkh_in{1,2,10}[_policy]` | P2PKH spends, `SIGHASH_ALL\|FORKID` |
| `p2pkh_in10_badsig_last` | control: last signature over a different digest, must be rejected |
| `p2pkh_in{1000,8000,86584}` | P2PKH spends with many inputs |
| `sha256_preimage_{100KB,1MB}` | `OP_SHA256 <h> OP_EQUAL` spent by pushing the preimage |
| `p2pkh_lockscript100KB_in10` | P2PKH behind a 100 KB pushed-and-dropped prefix, so each signature digest covers 100 KB of script |
| `outputs1000x1KB_in{1,10}` | 1,000 data outputs of 1 KB each |
| `sighash_original_in{100,1000,3000}` | P2PKH spends signed with sighash type `0x61`, which selects the original (pre-BIP143) digest after Chronicle activation |

## Parallel input verification

`build.sh v1.4.0 pr67=<commit>` builds a binary against a gobdk commit that has
`script.SetParallelInputHelpers`; `-helpers N` sets it before the first
validation (`-1` automatic). `run67.sh` compares v1.4.0 with that build at
helpers 0, 2 and automatic, unpinned at `GOMAXPROCS` = online CPUs, and runs:

- the shapes above plus `p2pkh_in{15,16,17,32}`, `p2pkh_in8000_badsig_{first,last}`,
  `p2pkh_lockscript100KB_in64_badsig_first`, `p2pkh_lockscript10KB_in1000` and
  `sha256_preimage10KB_in1000`;
- `-mode concurrency`: N goroutines validating the 10-input tx in a loop, plus one
  validating the 8,000-input tx, all sharing one `TxValidator`, for a fixed window;
  reports small-tx throughput and large-tx latency;
- with `GOLOAD_DIR` set, the `GoLoad` benchmark from that checkout's `cgobench`.
