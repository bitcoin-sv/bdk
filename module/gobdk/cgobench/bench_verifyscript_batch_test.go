package cgobench

import (
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// Helper function to benchmark batch verification with a specific batch size
func benchmarkVerifyScriptBatchSize(b *testing.B, batchSize int) {
	b.ReportAllocs()

	// Pre-construct the batch ONCE (outside of timing)
	batch := bdkscript.NewVerifyBatch(batchSize)
	for j := 0; j < batchSize; j++ {
		batch.Add(txBinExtended, dataUTXOHeights, blockHeight, true, nil)
	}

	// Warmup
	for i := 0; i < 10; i++ {
		results := se.VerifyScriptBatch(batch)
		if results == nil {
			b.Fatalf("VerifyScriptBatch returned nil during warmup")
		}
		if len(results) != batchSize {
			b.Fatalf("VerifyScriptBatch returned wrong number of results during warmup: got %d, want %d", len(results), batchSize)
		}
		for idx, err := range results {
			if err != nil && err.Code() != bdkscript.SCRIPT_ERR_OK {
				b.Fatalf("VerifyScriptBatch failed during warmup at index %d: %v", idx, err)
			}
		}
	}

	b.ResetTimer()

	// Actual benchmark - ONLY measure VerifyScriptBatch call
	for i := 0; i < b.N; i++ {
		results := se.VerifyScriptBatch(batch)
		// Minimal validation to prevent compiler optimization
		if results == nil || len(results) != batchSize {
			b.Fatalf("VerifyScriptBatch returned invalid results at iteration %d", i)
		}
	}
}

// BenchmarkVerifyScriptBatch_10 measures batch verification with 10 transactions
func BenchmarkVerifyScriptBatch_10(b *testing.B) {
	benchmarkVerifyScriptBatchSize(b, 10)
}

// BenchmarkVerifyScriptBatch_100 measures batch verification with 100 transactions
func BenchmarkVerifyScriptBatch_100(b *testing.B) {
	benchmarkVerifyScriptBatchSize(b, 100)
}

// BenchmarkVerifyScriptBatch_1000 measures batch verification with 1000 transactions
func BenchmarkVerifyScriptBatch_1000(b *testing.B) {
	benchmarkVerifyScriptBatchSize(b, 1000)
}

// BenchmarkVerifyScriptBatch_10000 measures batch verification with 10000 transactions
func BenchmarkVerifyScriptBatch_10000(b *testing.B) {
	benchmarkVerifyScriptBatchSize(b, 10000)
}

// BenchmarkVerifyScriptBatch measures batch verification performance (default 1000)
// Pre-constructs the batch ONCE, then ONLY measures the VerifyScriptBatch call
func BenchmarkVerifyScriptBatch(b *testing.B) {
	benchmarkVerifyScriptBatchSize(b, 1000)
}

// BenchmarkVerifyScriptBatch_NoConsensus measures batch verification without consensus
func BenchmarkVerifyScriptBatch_NoConsensus(b *testing.B) {
	b.ReportAllocs()

	const batchSize = 1000

	// Pre-construct the batch ONCE (outside of timing)
	batch := bdkscript.NewVerifyBatch(batchSize)
	for j := 0; j < batchSize; j++ {
		batch.Add(txBinExtended, dataUTXOHeights, blockHeight, false, nil)
	}

	// Warmup
	for i := 0; i < 10; i++ {
		results := se.VerifyScriptBatch(batch)
		if results == nil {
			b.Fatalf("VerifyScriptBatch returned nil during warmup")
		}
		if len(results) != batchSize {
			b.Fatalf("VerifyScriptBatch returned wrong number of results during warmup: got %d, want %d", len(results), batchSize)
		}
		for idx, err := range results {
			if err != nil && err.Code() != bdkscript.SCRIPT_ERR_OK {
				b.Fatalf("VerifyScriptBatch failed during warmup at index %d: %v", idx, err)
			}
		}
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		results := se.VerifyScriptBatch(batch)
		// Minimal validation to prevent compiler optimization
		if results == nil || len(results) != batchSize {
			b.Fatalf("VerifyScriptBatch returned invalid results at iteration %d", i)
		}
	}
}
