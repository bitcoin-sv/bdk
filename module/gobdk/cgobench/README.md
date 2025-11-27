# CGO and Script Verification Benchmark Suite

This package contains comprehensive benchmarks measuring:
1. **CGO overhead** - The cost of crossing the Go↔C boundary
2. **Script verification performance** - Real-world VerifyScript performance in Go vs C++

## Benchmark Files

- **bench_cgosimple_test.go**: Simple CGO overhead benchmarks (no-op calls, byte summing)
- **bench_verifyscript_test.go**: Go implementation of script verification benchmark
- **../../../example/bench_verifyscript.cpp**: C++ implementation of script verification benchmark

## Test Data

All script verification benchmarks use the same hardcoded transaction:
- **TXID**: d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7
- **Block**: 620940
- **Size**: 192 bytes (1 input, 1 output, no OP_RETURN)
- **UTXO Heights**: 574441

This is a real mainnet transaction representing a simple Alice→Bob payment.

---

## Running Benchmarks

### 1. Go Benchmarks (All)

```bash
cd /home/ctnguyen/development/bitcoin-sv/bdk/module/gobdk/cgobench

# Run all benchmarks with default timing (auto-determined)
go test -bench=.

# Run all benchmarks with longer time for accuracy (3 seconds each)
go test -bench=. -benchtime=3s

# Run all benchmarks with fixed iteration count
go test -bench=. -benchtime=100000x

# Show memory allocations
go test -bench=. -benchmem
```

### 2. Go Benchmarks (Individual)

```bash
# CGO simple benchmarks
go test -bench=BenchmarkGoNoOp
go test -bench=BenchmarkCGoNoOp
go test -bench=BenchmarkCGoSumBytes_1KB
go test -bench=BenchmarkCGoSumBytes_10KB
go test -bench=BenchmarkCGoSumBytes_100KB
go test -bench=BenchmarkGoSumBytes_1KB

# Script verification benchmarks
go test -bench=BenchmarkVerifyScript
go test -bench=BenchmarkVerifyScript_NoConsensus

# Run specific benchmark with custom time/iterations
go test -bench=BenchmarkVerifyScript -benchtime=10s
go test -bench=BenchmarkVerifyScript -benchtime=100000x
```

### 3. C++ Benchmark

```bash
# Assuming you've built the project, the binary is at:
BENCH_BIN=/home/ctnguyen/development/bitcoin-sv/build/x64/release/bench_verifyscript

# Run with default iterations (10,000)
$BENCH_BIN

# Run with custom iterations
$BENCH_BIN -i 100000
$BENCH_BIN -i 1000000

# Run without consensus checking
$BENCH_BIN -i 100000 -c
$BENCH_BIN --iterations=100000 --disable-consensus

# Show help
$BENCH_BIN -h
```

To build the C++ benchmark:
```bash
cd /home/ctnguyen/development/bitcoin-sv/build/x64/release
cmake --build . --target bench_verifyscript
```

---

## Benchmark Results

### CGO Simple Overhead (3s runtime)

```
BenchmarkGoNoOp-16                      	1000000000	         0.24 ns/op
BenchmarkCGoNoOp-16                     	62168971	        53.88 ns/op
BenchmarkCGoSumBytes_1KB-16             	 6705693	       574.6 ns/op
BenchmarkCGoSumBytes_10KB-16            	  684961	      5559 ns/op
BenchmarkCGoSumBytes_100KB-16           	   64915	     58355 ns/op
BenchmarkGoSumBytes_1KB-16              	13890534	       265.2 ns/op
```

**CGO Boundary Crossing Overhead: ~53.88 ns**

This overhead comes from:
- Go runtime preparing for C call
- Stack switching (Go uses segmented stacks, C uses fixed stacks)
- Register save/restore
- Scheduler coordination
- CPU pipeline/cache disruption

**Surprising Result: CGO + C code is slower than pure Go for simple operations**

For 1KB byte summing:
- **CGO + C**: 574.6 ns/op (53.88 ns CGO overhead + ~520 ns C execution)
- **Pure Go**: 265.2 ns/op

The C code runs **96% slower** than equivalent Go code! This is because:
1. **Pipeline disruption**: Stack switching causes CPU pipeline flush
2. **Cache effects**: Instruction cache misses after crossing boundary
3. **Branch predictor reset**: Prediction state lost across boundaries
4. **Memory prefetcher disruption**: Prefetcher doesn't predict across CGO calls

**Key Insight**: CGO overhead isn't just the ~54ns crossing time - it's also the microarchitectural performance degradation that makes C code run in a "cold" state.

---

### Script Verification Benchmarks (Release Build - 2025-11-27)

**Test Environment:**
- Build: Release (optimized, -O3)
- CPU: Intel Core i9-9980HK @ 2.40GHz
- Test Duration: ~10 seconds per benchmark
- Transaction: d43ad4d4...eae0bad7 (192 bytes, 1 input, 2 outputs)

#### Performance Results

| Mode | Batch Size | Implementation | Iterations | Total Txs | Time/tx (µs) | Throughput (tx/sec) |
|------|------------|----------------|------------|-----------|--------------|---------------------|
| **Single** | 1 | **C++** | 10,000 | 10,000 | **41.05** | **24,373** |
| **Single** | 1 | **Go** | 10,000 | 10,000 | **46.85** | **21,415** |
| Batch | 10 | C++ | 1,000 | 10,000 | 48.72 | 20,526 |
| Batch | 10 | Go | 1,000 | 10,000 | 47.42 | 21,090 |
| Batch | 100 | C++ | 100 | 10,000 | 48.52 | 20,608 |
| Batch | 100 | Go | 100 | 10,000 | 45.87 | 21,803 |
| Batch | 1000 | C++ | 10 | 10,000 | 44.23 | 22,611 |
| Batch | 1000 | Go | 10 | 10,000 | 47.46 | 21,072 |
| Batch | 10000 | C++ | 1 | 10,000 | 47.75 | 20,943 |
| Batch | 10000 | Go | 1 | 10,000 | 77.01 | 12,986 |

---

## Performance Analysis

### 1. CGO Overhead: Go Single vs C++ Single

| Implementation | Time/tx (µs) | Throughput (tx/sec) | Overhead |
|----------------|--------------|---------------------|----------|
| **C++ Single** | 41.05 | 24,373 | baseline |
| **Go Single** | 46.85 | 21,415 | +5.80 µs (+14.1%) |

**Result**: Go via CGO has 14.1% overhead compared to native C++. Go achieves 88% of C++ performance.

**Analysis**: The CGO overhead is relatively small for this workload. The script verification operation is complex enough (~41-47 µs) that the CGO call overhead (~5.8 µs) represents about 14% of total execution time. This includes the boundary crossing cost (~20-50 ns) plus any data marshaling and microarchitectural effects.

### 2. Batch Performance: Optimal Batch Sizes

**C++ Batch Performance:**

| Config | Time/tx (µs) | vs C++ Single (41.05 µs) | Change |
|--------|--------------|--------------------------|--------|
| **C++ Single** | 41.05 | baseline | - |
| C++ Batch-10 | 48.72 | +7.67 µs | 18.7% slower |
| C++ Batch-100 | 48.52 | +7.47 µs | 18.2% slower |
| C++ Batch-1000 | 44.23 | +3.18 µs | 7.7% slower |
| C++ Batch-10000 | 47.75 | +6.70 µs | 16.3% slower |

**Result**: C++ batch mode is slower than single mode for all batch sizes. Best batch performance is with batch size of 1000 (only 7.7% slower than single mode).

**Go Batch Performance:**

| Config | Time/tx (µs) | vs Go Single (46.85 µs) | Change |
|--------|--------------|------------------------|--------|
| **Go Single** | 46.85 | baseline | - |
| Go Batch-10 | 47.42 | +0.57 µs | 1.2% slower |
| Go Batch-100 | 45.87 | -0.98 µs | 2.1% faster |
| Go Batch-1000 | 47.46 | +0.61 µs | 1.3% slower |
| Go Batch-10000 | 77.01 | +30.16 µs | 64.4% slower |

**Result**: Go batch mode performs similarly to single mode for small-to-medium batches (10-1000), with batch size of 100 being 2.1% faster. Very large batches (10000) degrade significantly (64.4% slower).

### 3. Batch Performance Analysis

**Batch Mode Performance:**

The batch mode shows mixed results compared to single-transaction mode:
- **C++ Batch**: 7.7-18.7% slower than single mode (best: 1000 tx batch)
- **Go Batch**: 2.1% faster to 1.3% slower for small-medium batches (best: 100 tx batch)

**Why is batch mode slower for C++?**
The current implementation may have overhead from:
1. **Batch construction**: Creating and managing batch data structures
2. **Iterator overhead**: Sequential loop through batch elements
3. **Memory access patterns**: Less optimal cache behavior compared to optimized single-call path

**Large Batch Degradation (10K transactions):**

Both implementations degrade at 10K batch size:
- **C++**: 16.3% slower than single
- **Go**: 64.4% slower than single

This is likely due to:
1. **Memory pressure**: 10K transactions × ~200 bytes = ~2 MB of data
2. **Cache thrashing**: Working set exceeds L2/L3 cache capacity
3. **Go GC pressure** (Go only): Large batch object may trigger garbage collection

**Optimal Batch Sizes:**
- **C++**: Use single-transaction mode (best performance) or batch size 1000 (7.7% slower)
- **Go**: Use single-transaction mode or batch size 100 (2.1% faster)
- **Avoid**: Batches >1000 (significant performance degradation)

---

## Key Takeaways

### 1. CGO Overhead is Small but Measurable

For complex operations like script verification (~41-47 µs), the CGO overhead is relatively small:
- **C++ Single**: 41.05 µs/tx (24,373 tx/sec)
- **Go Single**: 46.85 µs/tx (21,415 tx/sec)
- **CGO Overhead**: +5.80 µs (+14.1%)

**Key Insight**: Go achieves 88% of native C++ performance. The 14% overhead comes from CGO boundary crossing (~20-50 ns), data marshaling, and microarchitectural effects. This is acceptable overhead for most production workloads.

### 2. Single benchmark is more consistent than batch benchmark

The benchmark results show that single-transaction mode provides the best consistancy:
- **C++ Single**: 41.05 µs/tx
- **Go Single**: 46.85 µs/tx

Batch mode shows mixed results:
- **C++ Batch**: 7.7-18.7% slower than single mode
- **Go Batch**: Mostly equivalent to single mode (2.1% faster at best)
- **Large batches (10K)**: Significant degradation for both (16-64% slower)

### 3. Benchmark Variability and CGO Overhead Sources

**Important Note on Result Variability:**

Despite the typical results shown above, different benchmark runs can produce varying outcomes. Sometimes Go can even appear faster than C++ in certain runs. This **non-deterministic behavior** is due to:

1. **Unmanaged Heap Allocations**: Deep within the C++ core functions, the creation and deserialization of `CTransaction` objects involve numerous heap allocations that are not under our direct control. Each `VerifyScript` call triggers transaction deserialization from binary format, which allocates memory dynamically.

2. **Memory Allocator State**: The performance of heap allocation depends on the allocator's internal state (fragmentation, free list state, cache locality), which varies between runs.

3. **CPU State Variability**: Cache warming, branch predictor state, CPU frequency scaling, and other microarchitectural factors can vary between runs.

4. **System Background Activity**: OS scheduling, other processes, interrupts, and system services can affect timing measurements.

**Sources of CGO Overhead:**

The 14% CGO overhead observed in Go comes from multiple sources:

1. **Boundary Crossing Cost** (~20-50 ns per call):
   - Stack switching between Go's segmented stacks and C's fixed stacks
   - Register save/restore operations
   - Runtime coordination between Go scheduler and C execution

2. **Data Marshaling**:
   - Converting Go slices to C pointers (`[]byte` to `const uint8_t*`)
   - Copying or pinning Go memory to prevent garbage collection during C execution
   - Converting C return values back to Go types

3. **Microarchitectural Effects**:
   - CPU pipeline flush when crossing the Go↔C boundary
   - Instruction cache misses (switching between Go and C code)
   - Branch predictor reset across language boundaries
   - Memory prefetcher disruption

4. **Memory Allocation Patterns**:
   - The C++ implementation performs extensive heap allocations during transaction deserialization
   - Go's garbage collector may interact with C memory allocations, adding overhead
   - Cache effects from allocation patterns differ between pure C++ and Go→C paths

**Recommendation**: When benchmarking CGO performance, always run multiple iterations and consider the average/median rather than a single run. The variability is inherent to the current implementation's reliance on dynamic memory allocation.

---

## Running the Benchmarks

**C++ Benchmarks:**
```bash
cd /home/ctnguyen/development/bitcoin-sv/build

# Single mode (10,000 iterations)
./x64/release/bench_verifyscript_single -i 10000

# Batch modes
./x64/release/bench_verifyscript_batch -i 1000 -b 10
./x64/release/bench_verifyscript_batch -i 100 -b 100
./x64/release/bench_verifyscript_batch -i 10 -b 1000
./x64/release/bench_verifyscript_batch -i 1 -b 10000
```

**Go Benchmarks:**
```bash
cd /home/ctnguyen/development/bitcoin-sv/bdk/module/gobdk/cgobench

# Single mode (10,000 iterations)
go test -bench=BenchmarkVerifyScript_Single$ -benchtime=10000x -run=^$ -benchmem

# Batch modes
go test -bench=BenchmarkVerifyScriptBatch_10$ -benchtime=1000x -run=^$ -benchmem
go test -bench=BenchmarkVerifyScriptBatch_100$ -benchtime=100x -run=^$ -benchmem
go test -bench=BenchmarkVerifyScriptBatch_1000$ -benchtime=10x -run=^$ -benchmem
go test -bench=BenchmarkVerifyScriptBatch_10000$ -benchtime=1x -run=^$ -benchmem
```

---

## Memory Profiling (C++ Debug Builds)

Memory profiling was performed using Valgrind on debug builds to understand allocation patterns and confirm the extensive heap allocation behavior mentioned above.

**Note**: Memory profiling data is from debug builds and should NOT be compared with release build performance data. Memory allocation counts help understand implementation behavior but don't directly correlate with release build performance.

### Test Configuration

| Test | Mode | Batch Size | Iterations | Total Transactions |
|------|------|------------|------------|-------------------|
| **Test 1** | Single | 1 tx/call | 1000 calls | 1000 |
| **Test 2** | Batch | 1000 tx/batch | 1 call | 1000 |

**Commands used:**
```bash
# Test 1: Single mode - 1000 iterations of single transaction verification
valgrind --tool=dhat ./x64/debug/bench_verifyscript_singled -i 1000

# Test 2: Batch mode - 1 batch of 1000 transactions
valgrind --tool=dhat ./x64/debug/bench_verifyscript_batchd -i 1 -b 1000
```

### DHAT Results: Allocation Counts

| Metric | Test 1: Single (1000 tx) | Test 2: Batch (1000 tx) | Ratio |
|--------|--------------------------|-------------------------|-------|
| **Total malloc/new calls** | 30,813 | 258,560 | 8.39× |
| **Total bytes allocated** | 2,087,582 | 16,317,128 | 7.82× |
| **Allocations per transaction** | 30.8 | 258.6 | **8.39× worse** |
| **Bytes per transaction** | 2,088 | 16,317 | **7.82× worse** |
| **Memory reads per tx** | 29.8 KB | 283.5 KB | **9.51× worse** |
| **Memory writes per tx** | 8.4 KB | 77.2 KB | **9.19× worse** |

### Massif Results: Heap Memory

| Metric | Test 1: Single (1000 tx) | Test 2: Batch (1000 tx) | Ratio |
|--------|--------------------------|-------------------------|-------|
| **Peak heap size** | 535.4 KB | 594.8 KB | 1.11× |
| **Total bytes allocated** | 2.09 MB | 16.32 MB | **7.82× worse** |
| **Heap at program end** | 0 bytes (all freed) | 0 bytes (all freed) | - |

### Key Findings

**Extensive Heap Allocation Chaos Confirmed:**

The Valgrind profiling reveals extreme heap allocation behavior in transaction verification:
- **31 heap allocations per transaction** in single mode
- **259 heap allocations per transaction** in batch mode (**8.4× more**)

This extensive allocation is due to transaction deserialization. From the C++ source code, each `VerifyScript` call deserializes the transaction from binary format:

```cpp
// From scriptengine.cpp
ScriptError VerifyScript(..., std::span<const uint8_t> extendedTX, ...) {
    // Deserialize transaction from binary EVERY TIME
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    tx_stream >> eTX;  // HEAP ALLOCATIONS HERE

    const CTransaction ctx(eTX.mtx);  // MORE HEAP ALLOCATIONS
    ...
}
```

**Why This Causes Performance Variability:**

1. **Allocator State Dependency**: Each heap allocation's performance depends on the allocator's current state (free list, fragmentation level)
2. **Cache Effects**: Memory allocation patterns affect CPU cache behavior, which varies between runs
3. **Non-deterministic Timing**: The combination of ~100+ allocations per transaction with varying allocator state creates non-deterministic performance

**Batch Mode Heap Allocation Chaos:**

Batch mode exhibits dramatically worse memory behavior per transaction:
- **8.4× more allocations per transaction** (259 vs 31)
- **7.8× more bytes allocated per transaction** (16 KB vs 2 KB)
- **9.5× more memory reads per transaction** (284 KB vs 30 KB)
- **9.2× more memory writes per transaction** (77 KB vs 8 KB)