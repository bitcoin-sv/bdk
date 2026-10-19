package cgobench

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"testing"

	"github.com/bitcoin-sv/bdk/module/gobdk/secp256k1"
)

// Synthetic P2PKH spends with many inputs, for measuring how ValidateTransaction
// scales with the input count (bitcoin-sv/bdk#64). Every input spends output i of
// one fake previous tx and is locked to the same key; the tx has one output.
const (
	inputsBlockHeight  = 700000 // post-Genesis, pre-Chronicle on mainnet
	inputsUTXOHeight   = 699999
	inputsSatoshis     = 100000
	inputsOutSatoshis  = 1000
	inputsSigHashType  = 0x41 // SIGHASH_ALL | SIGHASH_FORKID
	inputsTxVersion    = 2
	inputsTxLockTime   = 0
	inputsSequence     = 0xffffffff
	inputsPubKeyHex    = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798" // private key 1
	inputsPubKeyHash   = "751e76e8199196d454941c45d1b3a323f1433bd6"                           // HASH160 of inputsPubKeyHex
	inputsPrevTxIDByte = 0x01
)

func BenchmarkValidateTransaction_Inputs(b *testing.B) {
	for _, nInputs := range []int{1000, 2000, 4000, 8000} {
		b.Run(fmt.Sprintf("inputs=%d", nInputs), func(b *testing.B) {
			etx, utxoHeights := makeManyInputP2PKHTx(b, nInputs)

			// One unmeasured call also checks the synthetic tx is valid.
			if err := se.ValidateTransaction(etx, utxoHeights, inputsBlockHeight, true); err != nil {
				b.Fatalf("ValidateTransaction failed on %d inputs: %v", nInputs, err)
			}

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				if err := se.ValidateTransaction(etx, utxoHeights, inputsBlockHeight, true); err != nil {
					b.Fatalf("ValidateTransaction failed on %d inputs: %v", nInputs, err)
				}
			}
			b.ReportMetric(float64(b.Elapsed().Microseconds())/float64(b.N)/float64(nInputs), "us/input")
		})
	}
}

// makeManyInputP2PKHTx returns the extended serialisation of a signed spend and
// its UTXO heights. The BIP143 midstate hashes are computed once so building an
// 8,000-input tx stays linear.
func makeManyInputP2PKHTx(b *testing.B, nInputs int) ([]byte, []int32) {
	b.Helper()

	pubKey := mustDecodeHex(b, inputsPubKeyHex)
	lockingScript := append(append([]byte{0x76, 0xa9, 0x14}, mustDecodeHex(b, inputsPubKeyHash)...), 0x88, 0xac)
	privKey := make([]byte, 32)
	privKey[31] = 1

	prevTxID := make([]byte, 32)
	prevTxID[0] = inputsPrevTxIDByte
	outpoint := func(i int) []byte {
		return binary.LittleEndian.AppendUint32(append([]byte{}, prevTxID...), uint32(i))
	}

	var prevouts, sequences bytes.Buffer
	for i := 0; i < nInputs; i++ {
		prevouts.Write(outpoint(i))
		_ = binary.Write(&sequences, binary.LittleEndian, uint32(inputsSequence))
	}
	var outputs bytes.Buffer
	_ = binary.Write(&outputs, binary.LittleEndian, uint64(inputsOutSatoshis))
	writeVarBytes(&outputs, lockingScript)

	hashPrevouts := sha256d(prevouts.Bytes())
	hashSequence := sha256d(sequences.Bytes())
	hashOutputs := sha256d(outputs.Bytes())

	var etx bytes.Buffer
	_ = binary.Write(&etx, binary.LittleEndian, uint32(inputsTxVersion))
	etx.Write([]byte{0, 0, 0, 0, 0, 0xef}) // extended format marker
	writeVarInt(&etx, uint64(nInputs))
	for i := 0; i < nInputs; i++ {
		var preimage bytes.Buffer
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(inputsTxVersion))
		preimage.Write(hashPrevouts)
		preimage.Write(hashSequence)
		preimage.Write(outpoint(i))
		writeVarBytes(&preimage, lockingScript)
		_ = binary.Write(&preimage, binary.LittleEndian, uint64(inputsSatoshis))
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(inputsSequence))
		preimage.Write(hashOutputs)
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(inputsTxLockTime))
		_ = binary.Write(&preimage, binary.LittleEndian, uint32(inputsSigHashType))

		sig, err := secp256k1.SignMessage(sha256d(preimage.Bytes()), privKey)
		if err != nil {
			b.Fatalf("signing input %d: %v", i, err)
		}
		sig = append(sig, inputsSigHashType)

		var scriptSig bytes.Buffer
		writeVarBytes(&scriptSig, sig) // a push of up to 75 bytes is its own length opcode
		writeVarBytes(&scriptSig, pubKey)

		etx.Write(outpoint(i))
		writeVarBytes(&etx, scriptSig.Bytes())
		_ = binary.Write(&etx, binary.LittleEndian, uint32(inputsSequence))
		_ = binary.Write(&etx, binary.LittleEndian, uint64(inputsSatoshis))
		writeVarBytes(&etx, lockingScript)
	}
	writeVarInt(&etx, 1)
	etx.Write(outputs.Bytes())
	_ = binary.Write(&etx, binary.LittleEndian, uint32(inputsTxLockTime))

	utxoHeights := make([]int32, nInputs)
	for i := range utxoHeights {
		utxoHeights[i] = inputsUTXOHeight
	}
	return etx.Bytes(), utxoHeights
}

func sha256d(data []byte) []byte {
	first := sha256.Sum256(data)
	second := sha256.Sum256(first[:])
	return second[:]
}

func writeVarInt(w *bytes.Buffer, n uint64) {
	switch {
	case n < 0xfd:
		w.WriteByte(byte(n))
	case n <= 0xffff:
		w.WriteByte(0xfd)
		_ = binary.Write(w, binary.LittleEndian, uint16(n))
	case n <= 0xffffffff:
		w.WriteByte(0xfe)
		_ = binary.Write(w, binary.LittleEndian, uint32(n))
	default:
		w.WriteByte(0xff)
		_ = binary.Write(w, binary.LittleEndian, n)
	}
}

func writeVarBytes(w *bytes.Buffer, data []byte) {
	writeVarInt(w, uint64(len(data)))
	w.Write(data)
}

func mustDecodeHex(b *testing.B, s string) []byte {
	b.Helper()
	data, err := hex.DecodeString(s)
	if err != nil {
		b.Fatalf("decoding %q: %v", s, err)
	}
	return data
}
