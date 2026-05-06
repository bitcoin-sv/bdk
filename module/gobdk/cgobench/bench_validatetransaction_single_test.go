package cgobench

import (
	"testing"
)

// BenchmarkValidateTransaction_Single measures the performance of ValidateTransaction
// This is the Go equivalent of bench_validatetransaction_single.cpp
// ONLY measures the ValidateTransaction call, not any setup
func BenchmarkValidateTransaction_Single(b *testing.B) {
	b.ReportAllocs()

	// Warmup (not measured)
	for i := 0; i < 100; i++ {
		err := se.ValidateTransaction(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("ValidateTransaction failed during warmup: %v", err)
		}
	}

	b.ResetTimer()

	// Actual benchmark - ONLY measure ValidateTransaction call
	for i := 0; i < b.N; i++ {
		err := se.ValidateTransaction(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("ValidateTransaction failed at iteration %d: %v", i, err)
		}
	}
}

// BenchmarkValidateTransaction_Single_NoConsensus measures ValidateTransaction without consensus checking
func BenchmarkValidateTransaction_Single_NoConsensus(b *testing.B) {
	b.ReportAllocs()

	// Warmup
	for i := 0; i < 100; i++ {
		err := se.ValidateTransaction(txBinExtended, dataUTXOHeights, blockHeight, false)
		if err != nil {
			b.Fatalf("ValidateTransaction failed during warmup: %v", err)
		}
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		err := se.ValidateTransaction(txBinExtended, dataUTXOHeights, blockHeight, false)
		if err != nil {
			b.Fatalf("ValidateTransaction failed at iteration %d: %v", i, err)
		}
	}
}
