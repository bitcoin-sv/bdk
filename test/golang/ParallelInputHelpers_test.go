package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"runtime"
	"syscall"
	"testing"
	"time"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/bitcoin-sv/bdk/module/gobdk/secp256k1"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Synthetic P2PKH spend with many inputs: every input spends output i of one fake
// previous tx and is locked to the key of private key 1; the tx has one output.
const (
	helpersBlockHeight = 700000 // post-Genesis, pre-Chronicle on mainnet
	helpersUTXOHeight  = 699999
	helpersSatoshis    = 100000
	helpersOutSatoshis = 1000
	helpersSigHashType = 0x41 // SIGHASH_ALL | SIGHASH_FORKID
	helpersSequence    = 0xffffffff
	helpersPubKeyHex   = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
	helpersPubKeyHash  = "751e76e8199196d454941c45d1b3a323f1433bd6"
)

// TestParallelInputHelpers runs in its own process (ctest runs each test file on
// its own), so the process-wide setting starts at its default here.
func TestParallelInputHelpers(t *testing.T) {
	t.Run("defaults to automatic and reads back", func(t *testing.T) {
		assert.Equal(t, int64(-1), goscript.GetParallelInputHelpers())
		defer goscript.SetParallelInputHelpers(-1)
		for _, helpers := range []int64{0, 1, 3, 1000, -1} {
			goscript.SetParallelInputHelpers(helpers)
			assert.Equal(t, helpers, goscript.GetParallelInputHelpers())
		}
		goscript.SetParallelInputHelpers(-7)
		assert.Equal(t, int64(-1), goscript.GetParallelInputHelpers(), "every negative value means automatic")
	})

	t.Run("results are the same at every setting", func(t *testing.T) {
		const nInputs = 64
		se := goscript.NewTxValidator("main")
		require.NotNil(t, se)
		valid, heights := makeHelpersP2PKHTx(t, nInputs, -1)
		broken, _ := makeHelpersP2PKHTx(t, nInputs, 37)

		defer goscript.SetParallelInputHelpers(-1)
		goscript.SetParallelInputHelpers(-1)
		require.NoError(t, se.ValidateTransaction(valid, heights, helpersBlockHeight, true))
		want := se.ValidateTransaction(broken, heights, helpersBlockHeight, true)
		require.Error(t, want)
		for _, helpers := range []int64{0, 1, 2, -1} {
			goscript.SetParallelInputHelpers(helpers)
			assert.NoError(t, se.ValidateTransaction(valid, heights, helpersBlockHeight, true), "helpers=%d", helpers)
			assert.Equal(t, want, se.ValidateTransaction(broken, heights, helpersBlockHeight, true), "helpers=%d", helpers)
		}
	})

	// With no helpers the whole validation runs on the calling thread, so the
	// process uses about one CPU-second per second while it runs. Only that side
	// is checked: how much faster the automatic setting is depends on how idle
	// the machine is.
	t.Run("no helpers verifies on the calling thread", func(t *testing.T) {
		const nInputs = 4000
		se := goscript.NewTxValidator("main")
		require.NotNil(t, se)
		etx, heights := makeHelpersP2PKHTx(t, nInputs, -1)

		defer goscript.SetParallelInputHelpers(-1)
		goscript.SetParallelInputHelpers(0)
		require.NoError(t, se.ValidateTransaction(etx, heights, helpersBlockHeight, true))
		runtime.GC()
		cpu0, start := processCPUTime(), time.Now()
		for i := 0; i < 3; i++ {
			require.NoError(t, se.ValidateTransaction(etx, heights, helpersBlockHeight, true))
		}
		wall, cpu := time.Since(start), processCPUTime()-cpu0
		assert.LessOrEqual(t, cpu.Seconds(), 1.5*wall.Seconds(),
			"%v of CPU in %v with no helpers: work ran on other threads", cpu, wall)
	})
}

func processCPUTime() time.Duration {
	var usage syscall.Rusage
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &usage); err != nil {
		return 0
	}
	return time.Duration(usage.Utime.Nano() + usage.Stime.Nano())
}

// makeHelpersP2PKHTx returns the extended serialisation of a signed spend with
// nInputs inputs and its UTXO heights. Input badInput (none if negative) carries
// a well-formed signature of the wrong message, so only ECDSA verification
// rejects it. The BIP143 midstate hashes are computed once.
func makeHelpersP2PKHTx(t *testing.T, nInputs int, badInput int) ([]byte, []int32) {
	t.Helper()
	decode := func(s string) []byte {
		data, err := hex.DecodeString(s)
		require.NoError(t, err)
		return data
	}
	sha256d := func(data []byte) []byte {
		first := sha256.Sum256(data)
		second := sha256.Sum256(first[:])
		return second[:]
	}
	writeVarInt := func(w *bytes.Buffer, n uint64) {
		switch {
		case n < 0xfd:
			w.WriteByte(byte(n))
		case n <= 0xffff:
			w.WriteByte(0xfd)
			_ = binary.Write(w, binary.LittleEndian, uint16(n))
		default:
			w.WriteByte(0xfe)
			_ = binary.Write(w, binary.LittleEndian, uint32(n))
		}
	}
	writeVarBytes := func(w *bytes.Buffer, data []byte) {
		writeVarInt(w, uint64(len(data)))
		w.Write(data)
	}

	pubKey := decode(helpersPubKeyHex)
	lockingScript := append(append([]byte{0x76, 0xa9, 0x14}, decode(helpersPubKeyHash)...), 0x88, 0xac)
	privKey := make([]byte, 32)
	privKey[31] = 1
	prevTxID := make([]byte, 32)
	prevTxID[0] = 0x01
	outpoint := func(i int) []byte {
		return binary.LittleEndian.AppendUint32(append([]byte{}, prevTxID...), uint32(i))
	}

	var prevouts, sequences, outputs bytes.Buffer
	for i := 0; i < nInputs; i++ {
		prevouts.Write(outpoint(i))
		_ = binary.Write(&sequences, binary.LittleEndian, uint32(helpersSequence))
	}
	_ = binary.Write(&outputs, binary.LittleEndian, uint64(helpersOutSatoshis))
	writeVarBytes(&outputs, lockingScript)
	hashPrevouts, hashSequence, hashOutputs := sha256d(prevouts.Bytes()), sha256d(sequences.Bytes()), sha256d(outputs.Bytes())

	var etx bytes.Buffer
	_ = binary.Write(&etx, binary.LittleEndian, uint32(2))
	etx.Write([]byte{0, 0, 0, 0, 0, 0xef}) // extended format marker
	writeVarInt(&etx, uint64(nInputs))
	for i := 0; i < nInputs; i++ {
		var preimage bytes.Buffer
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(2))
		preimage.Write(hashPrevouts)
		preimage.Write(hashSequence)
		preimage.Write(outpoint(i))
		writeVarBytes(&preimage, lockingScript)
		_ = binary.Write(&preimage, binary.LittleEndian, uint64(helpersSatoshis))
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(helpersSequence))
		preimage.Write(hashOutputs)
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(0))
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(helpersSigHashType))
		digest := sha256d(preimage.Bytes())
		if i == badInput {
			digest = sha256d(digest)
		}
		sig, err := secp256k1.SignMessage(digest, privKey)
		require.NoError(t, err)
		sig = append(sig, helpersSigHashType)

		var scriptSig bytes.Buffer
		writeVarBytes(&scriptSig, sig)
		writeVarBytes(&scriptSig, pubKey)
		etx.Write(outpoint(i))
		writeVarBytes(&etx, scriptSig.Bytes())
		_ = binary.Write(&etx, binary.LittleEndian, uint32(helpersSequence))
		_ = binary.Write(&etx, binary.LittleEndian, uint64(helpersSatoshis))
		writeVarBytes(&etx, lockingScript)
	}
	writeVarInt(&etx, 1)
	etx.Write(outputs.Bytes())
	_ = binary.Write(&etx, binary.LittleEndian, uint32(0))

	heights := make([]int32, nInputs)
	for i := range heights {
		heights[i] = helpersUTXOHeight
	}
	return etx.Bytes(), heights
}
