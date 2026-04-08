package cgobench

import (
	"runtime"
	"sync"
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// benchmarkVerifyScriptBatchGoRoutineSize splits batchSize transactions across
// min(GOMAXPROCS, batchSize) goroutines, each calling VerifyScriptBatch on its
// sub-batch sequentially. This measures Go-level parallelism vs C++-internal
// parallelism (VerifyScriptBatchParallel) and vs pure sequential batch.
func benchmarkVerifyScriptBatchGoRoutineSize(b *testing.B, batchSize int) {
	b.ReportAllocs()

	numRoutines := runtime.GOMAXPROCS(0)
	if numRoutines > batchSize {
		numRoutines = batchSize
	}

	txsPerRoutine := batchSize / numRoutines
	remainder := batchSize % numRoutines

	// Pre-construct sub-batches ONCE (outside of timing)
	subBatches := make([]*bdkscript.VerifyBatch, numRoutines)
	for r := 0; r < numRoutines; r++ {
		size := txsPerRoutine
		if r < remainder {
			size++
		}
		subBatches[r] = bdkscript.NewVerifyBatch(size)
		for j := 0; j < size; j++ {
			subBatches[r].Add(txBinExtended, dataUTXOHeights, blockHeight, true, nil)
		}
	}

	// Warmup
	for i := 0; i < 10; i++ {
		var wg sync.WaitGroup
		wg.Add(numRoutines)
		for r := 0; r < numRoutines; r++ {
			r := r
			go func() {
				defer wg.Done()
				se.VerifyScriptBatch(subBatches[r])
			}()
		}
		wg.Wait()
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		var wg sync.WaitGroup
		wg.Add(numRoutines)
		for r := 0; r < numRoutines; r++ {
			r := r
			go func() {
				defer wg.Done()
				results := se.VerifyScriptBatch(subBatches[r])
				if results == nil {
					b.Errorf("VerifyScriptBatch returned nil in goroutine %d", r)
				}
			}()
		}
		wg.Wait()
	}
}

func BenchmarkVerifyScriptBatchGoRoutine_10(b *testing.B) {
	benchmarkVerifyScriptBatchGoRoutineSize(b, 10)
}

func BenchmarkVerifyScriptBatchGoRoutine_100(b *testing.B) {
	benchmarkVerifyScriptBatchGoRoutineSize(b, 100)
}

func BenchmarkVerifyScriptBatchGoRoutine_1000(b *testing.B) {
	benchmarkVerifyScriptBatchGoRoutineSize(b, 1000)
}

func BenchmarkVerifyScriptBatchGoRoutine_10000(b *testing.B) {
	benchmarkVerifyScriptBatchGoRoutineSize(b, 10000)
}
