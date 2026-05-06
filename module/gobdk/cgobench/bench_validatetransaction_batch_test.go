package cgobench

import (
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// Helper function to benchmark batch validation with a specific batch size
func benchmarkValidateBatchSize(b *testing.B, batchSize int) {
	b.ReportAllocs()

	// Pre-construct the batch ONCE (outside of timing)
	batch := bdkscript.NewValidateBatch(batchSize)
	for j := 0; j < batchSize; j++ {
		batch.Add(txBinExtended, dataUTXOHeights, blockHeight, true)
	}

	// Warmup
	for i := 0; i < 10; i++ {
		results := se.ValidateBatch(batch)
		if results == nil {
			b.Fatalf("ValidateBatch returned nil during warmup")
		}
		if len(results) != batchSize {
			b.Fatalf("ValidateBatch returned wrong number of results during warmup: got %d, want %d", len(results), batchSize)
		}
		for idx, err := range results {
			if err != nil && err.Code() != bdkscript.SCRIPT_ERR_OK {
				b.Fatalf("ValidateBatch failed during warmup at index %d: %v", idx, err)
			}
		}
	}

	b.ResetTimer()

	// Actual benchmark - ONLY measure ValidateBatch call
	for i := 0; i < b.N; i++ {
		results := se.ValidateBatch(batch)
		// Minimal validation to prevent compiler optimization
		if results == nil || len(results) != batchSize {
			b.Fatalf("ValidateBatch returned invalid results at iteration %d", i)
		}
	}
}

// BenchmarkValidateBatch_10 measures batch validation with 10 transactions
func BenchmarkValidateBatch_10(b *testing.B) {
	benchmarkValidateBatchSize(b, 10)
}

// BenchmarkValidateBatch_100 measures batch validation with 100 transactions
func BenchmarkValidateBatch_100(b *testing.B) {
	benchmarkValidateBatchSize(b, 100)
}

// BenchmarkValidateBatch_1000 measures batch validation with 1000 transactions
func BenchmarkValidateBatch_1000(b *testing.B) {
	benchmarkValidateBatchSize(b, 1000)
}

// BenchmarkValidateBatch_10000 measures batch validation with 10000 transactions
func BenchmarkValidateBatch_10000(b *testing.B) {
	benchmarkValidateBatchSize(b, 10000)
}

// BenchmarkValidateBatch measures batch validation performance (default 1000)
// Pre-constructs the batch ONCE, then ONLY measures the ValidateBatch call
func BenchmarkValidateBatch(b *testing.B) {
	benchmarkValidateBatchSize(b, 1000)
}

// BenchmarkValidateBatch_NoConsensus measures batch validation without consensus
func BenchmarkValidateBatch_NoConsensus(b *testing.B) {
	b.ReportAllocs()

	const batchSize = 1000

	// Pre-construct the batch ONCE (outside of timing)
	batch := bdkscript.NewValidateBatch(batchSize)
	for j := 0; j < batchSize; j++ {
		batch.Add(txBinExtended, dataUTXOHeights, blockHeight, false)
	}

	// Warmup
	for i := 0; i < 10; i++ {
		results := se.ValidateBatch(batch)
		if results == nil {
			b.Fatalf("ValidateBatch returned nil during warmup")
		}
		if len(results) != batchSize {
			b.Fatalf("ValidateBatch returned wrong number of results during warmup: got %d, want %d", len(results), batchSize)
		}
		for idx, err := range results {
			if err != nil && err.Code() != bdkscript.SCRIPT_ERR_OK {
				b.Fatalf("ValidateBatch failed during warmup at index %d: %v", idx, err)
			}
		}
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		results := se.ValidateBatch(batch)
		// Minimal validation to prevent compiler optimization
		if results == nil || len(results) != batchSize {
			b.Fatalf("ValidateBatch returned invalid results at iteration %d", i)
		}
	}
}
