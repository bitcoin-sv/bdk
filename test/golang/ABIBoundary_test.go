package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"math"
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
	"github.com/libsv/go-bt/v2/bscript"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// A known-good mainnet extended transaction, TxID
// 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105.
const abiValidETxHEX = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"

var (
	abiValidUTXOHeights = []int32{631924, 631924}
	abiValidBlockHeight = int32(632099)
)

// varIntLen is the CompactSize width bitcoin uses for a byte count.
func varIntLen(n uint64) uint64 {
	switch {
	case n < 0xfd:
		return 1
	case n <= 0xffff:
		return 3
	case n <= 0xffffffff:
		return 5
	default:
		return 9
	}
}

// extendedLength applies the identity the issue states: the extended representation
// is the bare transaction, plus six marker bytes, plus per input eight amount bytes,
// the CompactSize length of the previous locking script, and the script itself.
func extendedLength(bareLen uint64, scriptLens []uint64) uint64 {
	total := bareLen + 6
	for _, s := range scriptLens {
		total += 8 + varIntLen(s) + s
	}
	return total
}

// aggregateScriptsFor picks three previous-locking-script sizes whose extended
// representation is exactly target, for the 143-byte bare transaction of the issue's
// construction. All three land above 0xffff, so each carries a 5-byte CompactSize.
func aggregateScriptsFor(target uint64) []uint64 {
	const bareLen = 143
	const perInput = 8 + 5 // amount bytes + 5-byte CompactSize
	sum := target - (bareLen + 6 + 3*perInput)
	each := sum / 3
	return []uint64{each, each, sum - 2*each}
}

// T-A1 — the direct proof that no narrowing is left at the boundary: a length crosses
// the C ABI and comes back unchanged, for values a C int could never have carried.
func TestABIEchoLengthCrossesUnchanged(t *testing.T) {
	lengths := []uint64{
		0,
		1,
		uint64(math.MaxInt32),
		uint64(math.MaxInt32) + 1,
		uint64(math.MaxUint32),
		uint64(math.MaxUint32) + 1,
		uint64(math.MaxInt64),
	}

	for _, n := range lengths {
		assert.Equal(t, n, goscript.ABIEchoLength(n), "length %d must cross the ABI unchanged", n)
	}
}

// T-A2 — no size ceiling was introduced: every non-negative length is accepted. A
// ceiling here could reject a consensus-valid spend, which is exactly what the fix
// must not do. A negative length is rejected with a typed error.
func TestCheckBufferLengthAppliesNoCeiling(t *testing.T) {
	accepted := []int{
		0,
		1,
		math.MaxInt32,
		math.MaxInt32 + 1,
		math.MaxUint32,
		math.MaxInt, // MaxInt64 on every platform bdk ships
	}

	for _, n := range accepted {
		assert.Nil(t, goscript.CheckBufferLength(n), "length %d must be accepted", n)
	}

	err := goscript.CheckBufferLength(-1)
	require.NotNil(t, err, "a negative length must be rejected")
	abiErr, ok := err.(goscript.ABIError)
	require.True(t, ok, "Expect ABIError type")
	assert.Equal(t, goscript.ABI_ERR_LENGTH_NEGATIVE, abiErr.Code())
}

// T-A3 — the issue's own aggregate construction, at each boundary it asks for. The
// lengths under test are the extended lengths of a 3-input, 143-byte-bare transaction
// whose previous locking scripts sum to just below, at, and above the boundary; only
// the arithmetic is performed, no buffer is allocated.
func TestAggregateBoundaryLengthsCrossTheABI(t *testing.T) {
	targets := map[string]uint64{
		"INT32_MAX":   uint64(math.MaxInt32),
		"INT32_MAX+1": uint64(math.MaxInt32) + 1,
		"UINT32_MAX":  uint64(math.MaxUint32),
	}

	for name, target := range targets {
		t.Run(name, func(t *testing.T) {
			scriptLens := aggregateScriptsFor(target)
			for _, s := range scriptLens {
				require.Greater(t, s, uint64(0xffff), "script must carry a 5-byte CompactSize")
				require.LessOrEqual(t, s, uint64(0xffffffff), "script must carry a 5-byte CompactSize")
			}

			assert.Equal(t, target, extendedLength(143, scriptLens),
				"the aggregate construction must reproduce the boundary length")
			assert.Nil(t, goscript.CheckBufferLength(int(target)),
				"the boundary length must be accepted by the guard")
			assert.Equal(t, target, goscript.ABIEchoLength(target),
				"the boundary length must cross the ABI unchanged")
		})
	}
}

// T-B1 — the new ABI domain did not swallow the parse path: an empty extended
// transaction is still a parse failure, not an ABI rejection.
func TestEmptyExtendedTransactionStillReportsException(t *testing.T) {
	se := goscript.NewTxValidator("main")
	require.NotNil(t, se)

	for _, eTx := range [][]byte{nil, {}} {
		err := se.ValidateTransaction(eTx, []int32{0}, int32(0), true)
		require.NotNil(t, err, "Expect error")
		scriptErr, ok := err.(goscript.ScriptError)
		require.True(t, ok, "Expect ScriptError type, got %T", err)
		assert.Equal(t, goscript.SCRIPT_ERR_CGO_EXCEPTION, scriptErr.Code())
	}
}

// T-B3 — the ASM entry points after the rename and the allocator/guard fixes:
// an empty script no longer panics, an invalid assembly gives an empty result, and a
// valid assembly still round-trips.
func TestASMBoundary(t *testing.T) {
	t.Run("empty script does not panic", func(t *testing.T) {
		assert.Equal(t, "", goscript.ToASM([]byte{}))
		assert.Equal(t, "", goscript.ToASM(nil))
	})

	t.Run("empty and invalid assembly give an empty script", func(t *testing.T) {
		assert.Equal(t, 0, len(goscript.FromASM("")))
		assert.Equal(t, 0, len(goscript.FromASM("mary had a little lamb")))
	})

	t.Run("valid assembly round-trips", func(t *testing.T) {
		const asmStr = "4 5 ADD 9 EQUAL"
		script := goscript.FromASM(asmStr)
		require.NotEqual(t, 0, len(script), "Expect a non empty script")
		assert.Equal(t, asmStr, goscript.ToASM(script))
	})
}

// T-B4 — Add reports whether the entry was appended, and the batch results stay
// positionally aligned with the entries the caller added.
func TestValidateBatchStaysPositional(t *testing.T) {
	se := goscript.NewTxValidator("main")
	require.NotNil(t, se)

	validETx, errHex := hex.DecodeString(abiValidETxHEX)
	require.Nil(t, errHex)

	entries := [][]byte{validETx, {0}, validETx}

	batch := goscript.NewValidateBatch(len(entries))
	require.NotNil(t, batch)

	for i, eTx := range entries {
		utxoHeights := abiValidUTXOHeights
		if i == 1 {
			utxoHeights = []int32{0}
		}
		assert.Nil(t, batch.Add(eTx, utxoHeights, abiValidBlockHeight, true),
			"Add must report success for entry %d", i)
	}

	assert.Equal(t, len(entries), batch.Size())

	results := se.ValidateBatch(batch)
	require.Equal(t, len(entries), len(results), "one result per entry added")
	assert.Nil(t, results[0], "the first valid entry must validate")
	assert.NotNil(t, results[1], "the malformed entry must fail")
	assert.Nil(t, results[2], "the third entry must validate, in its own position")
}

// conditionalDataScript builds OP_FALSE OP_IF OP_PUSHDATA4 <data> OP_ENDIF OP_TRUE,
// the low-opcount spendable form the issue uses to place bulk data in a previous
// locking script, sized to exactly totalLen bytes.
func conditionalDataScript(totalLen int) *bscript.Script {
	const wrapper = 9 // OP_FALSE OP_IF OP_PUSHDATA4 <4 length bytes> ... OP_ENDIF OP_TRUE
	payload := totalLen - wrapper

	raw := make([]byte, 0, totalLen)
	raw = append(raw, 0x00, 0x63, 0x4e) // OP_FALSE OP_IF OP_PUSHDATA4
	var size [4]byte
	binary.LittleEndian.PutUint32(size[:], uint32(payload))
	raw = append(raw, size[:]...)
	raw = append(raw, bytes.Repeat([]byte{0x01}, payload)...)
	raw = append(raw, 0x68, 0x51) // OP_ENDIF OP_TRUE

	script := bscript.Script(raw)
	return &script
}

// T-B5 — a transaction whose bare size is small but whose aggregate previous locking
// scripts are large, with script sizes straddling every CompactSize threshold. The
// extended length must satisfy the identity, and the validator must return a verdict
// rather than the exception domain, in both consensus modes.
func TestSmallBareLargeAggregate(t *testing.T) {
	// Sizes crossing 0xfc / 0xfd / 0xffff / 0x10000, plus one multi-megabyte script.
	scriptSizes := []int{252, 253, 0xffff, 0x10000, 8_000_000}

	tx := bt.NewTx()
	require.NotNil(t, tx)

	utxoHeights := make([]int32, 0, len(scriptSizes))
	scriptLens := make([]uint64, 0, len(scriptSizes))
	for i, size := range scriptSizes {
		txid := make([]byte, 32)
		txid[0] = byte(i + 1)

		script := conditionalDataScript(size)
		require.Equal(t, size, len(*script))

		require.Nil(t, tx.FromUTXOs(&bt.UTXO{
			TxID:          txid,
			Vout:          0,
			LockingScript: script,
			Satoshis:      1000,
		}))

		utxoHeights = append(utxoHeights, 700000)
		scriptLens = append(scriptLens, uint64(size))
	}

	opTrue := bscript.Script([]byte{0x51})
	tx.AddOutput(&bt.Output{Satoshis: 1, LockingScript: &opTrue})

	bare := tx.Bytes()
	eTx := tx.ExtendedBytes()

	assert.Equal(t, extendedLength(uint64(len(bare)), scriptLens), uint64(len(eTx)),
		"the extended length identity must hold")
	assert.Less(t, len(bare), 1000, "the bare transaction stays small")

	se := goscript.NewTxValidator("main")
	require.NotNil(t, se)

	for _, consensus := range []bool{true, false} {
		err := se.ValidateTransaction(eTx, utxoHeights, int32(800000), consensus)
		if err == nil {
			continue
		}

		if scriptErr, ok := err.(goscript.ScriptError); ok {
			assert.NotEqual(t, goscript.SCRIPT_ERR_CGO_EXCEPTION, scriptErr.Code(),
				"consensus=%v must give a verdict, not a CGO exception", consensus)
			continue
		}

		_, isDoS := err.(goscript.DoSError)
		assert.True(t, isDoS, "consensus=%v must give a verdict, got %T: %v", consensus, err, err)
	}
}
