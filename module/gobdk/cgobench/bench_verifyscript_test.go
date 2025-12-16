package cgobench

import (
	"encoding/hex"
	"testing"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
)

// Hardcoded transaction data from CSV:
// TXID: d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7
// Block: 620940
// Size: 192 bytes (1 input, 1 output, no OP_RETURN)
const (
	chainNet              = "main"
	blockHeight           = 620940
	txID                  = "d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7"
	txHexExtended         = "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000"
	utxoHeightsStr        = "574441"
)

var (
	txBinExtended   []byte
	dataUTXOHeights []int32
	se              *bdkscript.ScriptEngine
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

// BenchmarkVerifyScript measures the performance of VerifyScript
// This is the Go equivalent of bench_verifyscript.cpp
func BenchmarkVerifyScript(b *testing.B) {
	b.ReportAllocs()

	// Warmup (not measured)
	for i := 0; i < 100; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("VerifyScript failed during warmup: %v", err)
		}
	}

	b.ResetTimer()

	// Actual benchmark
	for i := 0; i < b.N; i++ {
		err := se.VerifyScript(txBinExtended, dataUTXOHeights, blockHeight, true)
		if err != nil {
			b.Fatalf("VerifyScript failed at iteration %d: %v", i, err)
		}
	}
}

// BenchmarkVerifyScript_NoConsensus measures VerifyScript without consensus checking
func BenchmarkVerifyScript_NoConsensus(b *testing.B) {
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
