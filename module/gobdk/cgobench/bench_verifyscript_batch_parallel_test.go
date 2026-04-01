package cgobench

import (
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// Helper function to benchmark parallel batch verification with a specific batch size
func benchmarkVerifyScriptBatchParallelSize(b *testing.B, batchSize int) {
	b.ReportAllocs()

	// Pre-construct the batch ONCE (outside of timing)
	batch := bdkscript.NewVerifyBatch(batchSize)
	for j := 0; j < batchSize; j++ {
		batch.Add(txBinExtended, dataUTXOHeights, blockHeight, true, nil)
	}

	// Warmup
	for i := 0; i < 10; i++ {
		results := se.VerifyScriptBatchParallel(batch, 0)
		if results == nil {
			b.Fatalf("VerifyScriptBatchParallel returned nil during warmup")
		}
		if len(results) != batchSize {
			b.Fatalf("VerifyScriptBatchParallel returned wrong number of results during warmup: got %d, want %d", len(results), batchSize)
		}
		for idx, err := range results {
			if err != nil && err.Code() != bdkscript.SCRIPT_ERR_OK {
				b.Fatalf("VerifyScriptBatchParallel failed during warmup at index %d: %v", idx, err)
			}
		}
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		results := se.VerifyScriptBatchParallel(batch, 0)
		if results == nil || len(results) != batchSize {
			b.Fatalf("VerifyScriptBatchParallel returned invalid results at iteration %d", i)
		}
	}
}

func BenchmarkVerifyScriptBatchParallel_10(b *testing.B) {
	benchmarkVerifyScriptBatchParallelSize(b, 10)
}

func BenchmarkVerifyScriptBatchParallel_100(b *testing.B) {
	benchmarkVerifyScriptBatchParallelSize(b, 100)
}

func BenchmarkVerifyScriptBatchParallel_1000(b *testing.B) {
	benchmarkVerifyScriptBatchParallelSize(b, 1000)
}

func BenchmarkVerifyScriptBatchParallel_10000(b *testing.B) {
	benchmarkVerifyScriptBatchParallelSize(b, 10000)
}
