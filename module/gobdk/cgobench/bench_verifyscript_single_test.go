package cgobench

import (
	"encoding/hex"
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
)

func init() {
	// Initialize once for all benchmarks
	var err error
	txBinExtended, err = hex.DecodeString(txHexExtended)
	if err != nil {
		panic("Failed to decode tx hex: " + err.Error())
	}

	// Parse transaction to verify it's valid
	tx, err := bt.NewTxFromString(txHexExtended)
	if err != nil {
		panic("Failed to parse transaction: " + err.Error())
	}

	if tx.TxID() != txID {
		panic("Transaction ID mismatch")
	}

	// Parse UTXO heights
	dataUTXOHeights = []int32{574441}

	// Initialize script engine
	se = bdkscript.NewScriptEngine(chainNet)
}

// BenchmarkVerifyScript_Single measures the performance of VerifyScript
// This is the Go equivalent of bench_verifyscript_single.cpp
// ONLY measures the VerifyScript call, not any setup
func BenchmarkVerifyScript_Single(b *testing.B) {
	b.ReportAllocs()

	// Warmup (not measured)
	for i := 0; i < 100; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("VerifyScript failed during warmup: %v", err)
		}
	}

	b.ResetTimer()

	// Actual benchmark - ONLY measure VerifyScript call
	for i := 0; i < b.N; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("VerifyScript failed at iteration %d: %v", i, err)
		}
	}
}

// BenchmarkVerifyScript_Single_NoConsensus measures VerifyScript without consensus checking
func BenchmarkVerifyScript_Single_NoConsensus(b *testing.B) {
	b.ReportAllocs()

	// Warmup
	for i := 0; i < 100; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, false)
		if err != nil {
			b.Fatalf("VerifyScript failed during warmup: %v", err)
		}
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, false)
		if err != nil {
			b.Fatalf("VerifyScript failed at iteration %d: %v", i, err)
		}
	}
}
