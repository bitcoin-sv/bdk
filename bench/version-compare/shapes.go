package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"math/rand/v2"

	"github.com/bitcoin-sv/bdk/module/gobdk/secp256k1"
)

// Heights at which every shape is validated. Mainnet is past both the Genesis
// (620538) and Chronicle (943816) activation heights, so this is the rule set
// current blocks are validated under.
const (
	genesisHeight   = 620538
	chronicleHeight = 943816
	blockHeight     = 972000
	utxoHeight      = 971999
)

const (
	txVersion  = 2
	txLockTime = 0
	sequence   = 0xffffffff
	inputSats  = 100000
	outputSats = 1000

	sigHashAllForkID   = 0x41 // SIGHASH_ALL | SIGHASH_FORKID: BIP143 digest
	sigHashAllOriginal = 0x61 // SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_CHRONICLE: original digest

	pubKeyHex  = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798" // private key 1
	pubKeyHash = "751e76e8199196d454941c45d1b3a323f1433bd6"                           // HASH160 of pubKeyHex
)

// shape is one transaction to validate. build returns the extended serialisation
// and the UTXO height of each input.
type shape struct {
	name      string
	group     string
	inputs    int
	consensus bool
	wantValid bool
	build     func() ([]byte, []int32, error)
}

func shapes() []shape {
	var s []shape
	add := func(name, group string, inputs int, consensus, wantValid bool, build func() (*tx, error)) {
		s = append(s, shape{name, group, inputs, consensus, wantValid, func() ([]byte, []int32, error) {
			t, err := build()
			if err != nil {
				return nil, nil, err
			}
			return t.extended(), t.utxoHeights(), nil
		}})
	}

	for _, n := range []int{1, 2, 10} {
		n := n
		add(fmt.Sprintf("p2pkh_in%d", n), "typical", n, true, true, func() (*tx, error) { return p2pkhTx(n, nil, 1, 0, -1) })
	}
	for _, n := range []int{1, 2, 10} {
		n := n
		add(fmt.Sprintf("p2pkh_in%d_policy", n), "typical", n, false, true, func() (*tx, error) { return p2pkhTx(n, nil, 1, 0, -1) })
	}
	// Control: the last input's signature is for a different digest, so the tx must be rejected.
	add("p2pkh_in10_badsig_last", "typical", 10, true, false, func() (*tx, error) { return p2pkhTx(10, nil, 1, 0, 9) })

	for _, n := range []int{1000, 8000, 86584} {
		n := n
		add(fmt.Sprintf("p2pkh_in%d", n), "large", n, true, true, func() (*tx, error) { return p2pkhTx(n, nil, 1, 0, -1) })
	}

	add("sha256_preimage_100KB", "hash", 1, true, true, func() (*tx, error) { return sha256PreimageTx(100 * 1024) })
	add("sha256_preimage_1MB", "hash", 1, true, true, func() (*tx, error) { return sha256PreimageTx(1024 * 1024) })
	add("p2pkh_lockscript100KB_in10", "hash", 10, true, true, func() (*tx, error) {
		return p2pkhTx(10, randomBytes(100*1024, 2), 1, 0, -1)
	})
	add("outputs1000x1KB_in1", "hash", 1, true, true, func() (*tx, error) { return p2pkhTx(1, nil, 1000, 1024, -1) })
	add("outputs1000x1KB_in10", "hash", 10, true, true, func() (*tx, error) { return p2pkhTx(10, nil, 1000, 1024, -1) })

	for _, n := range []int{100, 1000, 3000} {
		n := n
		add(fmt.Sprintf("sighash_original_in%d", n), "original_sighash", n, true, true, func() (*tx, error) { return originalSighashTx(n) })
	}
	return s
}

type txIn struct {
	outpoint    []byte
	scriptSig   []byte
	lockingCode []byte
	satoshis    uint64
	sequence    uint32
	utxoHeight  int32
}

type txOut struct {
	satoshis uint64
	script   []byte
}

type tx struct {
	version  uint32
	ins      []txIn
	outs     []txOut
	lockTime uint32
}

// extended returns the extended format: each input carries the satoshis and
// locking script of the output it spends.
func (t *tx) extended() []byte {
	var b bytes.Buffer
	_ = binary.Write(&b, binary.LittleEndian, t.version)
	b.Write([]byte{0, 0, 0, 0, 0, 0xef})
	writeVarInt(&b, uint64(len(t.ins)))
	for _, in := range t.ins {
		b.Write(in.outpoint)
		writeVarBytes(&b, in.scriptSig)
		_ = binary.Write(&b, binary.LittleEndian, in.sequence)
		_ = binary.Write(&b, binary.LittleEndian, in.satoshis)
		writeVarBytes(&b, in.lockingCode)
	}
	t.writeOutputs(&b)
	_ = binary.Write(&b, binary.LittleEndian, t.lockTime)
	return b.Bytes()
}

func (t *tx) writeOutputs(b *bytes.Buffer) {
	writeVarInt(b, uint64(len(t.outs)))
	for _, o := range t.outs {
		_ = binary.Write(b, binary.LittleEndian, o.satoshis)
		writeVarBytes(b, o.script)
	}
}

func (t *tx) utxoHeights() []int32 {
	h := make([]int32, len(t.ins))
	for i, in := range t.ins {
		h[i] = in.utxoHeight
	}
	return h
}

func outpoint(i int) []byte {
	prevTxID := make([]byte, 32)
	prevTxID[0] = 0x01
	return binary.LittleEndian.AppendUint32(prevTxID, uint32(i))
}

func p2pkhLockingScript(prefix []byte) []byte {
	var s bytes.Buffer
	if prefix != nil {
		writePush(&s, prefix)
		s.WriteByte(0x75) // OP_DROP
	}
	s.Write([]byte{0x76, 0xa9, 0x14}) // OP_DUP OP_HASH160 <20>
	s.Write(mustHex(pubKeyHash))
	s.Write([]byte{0x88, 0xac}) // OP_EQUALVERIFY OP_CHECKSIG
	return s.Bytes()
}

// p2pkhTx returns nIn signed P2PKH spends (BIP143 digest). A non-nil
// lockPrefix is pushed and dropped ahead of each locking script, which makes
// every input's scriptCode that much larger. nOut outputs of outSize bytes are
// OP_FALSE OP_RETURN data outputs; with outSize 0 there is one P2PKH output.
// badSig >= 0 signs that input over a different digest.
func p2pkhTx(nIn int, lockPrefix []byte, nOut, outSize, badSig int) (*tx, error) {
	lock := p2pkhLockingScript(lockPrefix)
	t := &tx{version: txVersion, lockTime: txLockTime}
	for i := 0; i < nIn; i++ {
		t.ins = append(t.ins, txIn{outpoint: outpoint(i), lockingCode: lock, satoshis: inputSats, sequence: sequence, utxoHeight: utxoHeight})
	}
	if outSize == 0 {
		t.outs = []txOut{{outputSats, p2pkhLockingScript(nil)}}
	} else {
		for i := 0; i < nOut; i++ {
			var s bytes.Buffer
			s.Write([]byte{0x00, 0x6a}) // OP_FALSE OP_RETURN
			writePush(&s, randomBytes(outSize, uint64(100+i)))
			t.outs = append(t.outs, txOut{0, s.Bytes()})
		}
	}

	var prevouts, sequences, outputs bytes.Buffer
	for _, in := range t.ins {
		prevouts.Write(in.outpoint)
		_ = binary.Write(&sequences, binary.LittleEndian, in.sequence)
	}
	for _, o := range t.outs {
		_ = binary.Write(&outputs, binary.LittleEndian, o.satoshis)
		writeVarBytes(&outputs, o.script)
	}
	hashPrevouts := sha256d(prevouts.Bytes())
	hashSequence := sha256d(sequences.Bytes())
	hashOutputs := sha256d(outputs.Bytes())

	for i := range t.ins {
		in := &t.ins[i]
		var p bytes.Buffer
		_ = binary.Write(&p, binary.LittleEndian, t.version)
		p.Write(hashPrevouts)
		p.Write(hashSequence)
		p.Write(in.outpoint)
		writeVarBytes(&p, in.lockingCode)
		_ = binary.Write(&p, binary.LittleEndian, in.satoshis)
		_ = binary.Write(&p, binary.LittleEndian, in.sequence)
		p.Write(hashOutputs)
		_ = binary.Write(&p, binary.LittleEndian, t.lockTime)
		_ = binary.Write(&p, binary.LittleEndian, uint32(sigHashAllForkID))
		digest := sha256d(p.Bytes())
		if i == badSig {
			digest = sha256d(append(p.Bytes(), 0))
		}
		scriptSig, err := signedScriptSig(digest, sigHashAllForkID)
		if err != nil {
			return nil, fmt.Errorf("input %d: %w", i, err)
		}
		in.scriptSig = scriptSig
	}
	return t, nil
}

// originalSighashTx returns nIn signed P2PKH spends using the original
// (pre-BIP143) digest, selected by sighash type 0x61 after Chronicle activation.
func originalSighashTx(nIn int) (*tx, error) {
	lock := p2pkhLockingScript(nil)
	t := &tx{version: txVersion, lockTime: txLockTime}
	for i := 0; i < nIn; i++ {
		t.ins = append(t.ins, txIn{outpoint: outpoint(i), lockingCode: lock, satoshis: inputSats, sequence: sequence, utxoHeight: utxoHeight})
	}
	t.outs = []txOut{{outputSats, p2pkhLockingScript(nil)}}

	var outs bytes.Buffer
	t.writeOutputs(&outs)

	for i := range t.ins {
		var p bytes.Buffer
		_ = binary.Write(&p, binary.LittleEndian, t.version)
		writeVarInt(&p, uint64(len(t.ins)))
		for j, in := range t.ins {
			p.Write(in.outpoint)
			if j == i {
				writeVarBytes(&p, in.lockingCode) // no OP_CODESEPARATOR to strip
			} else {
				p.WriteByte(0)
			}
			_ = binary.Write(&p, binary.LittleEndian, in.sequence)
		}
		p.Write(outs.Bytes())
		_ = binary.Write(&p, binary.LittleEndian, t.lockTime)
		_ = binary.Write(&p, binary.LittleEndian, uint32(sigHashAllOriginal))
		scriptSig, err := signedScriptSig(sha256d(p.Bytes()), sigHashAllOriginal)
		if err != nil {
			return nil, fmt.Errorf("input %d: %w", i, err)
		}
		t.ins[i].scriptSig = scriptSig
	}
	return t, nil
}

// sha256PreimageTx spends one OP_SHA256 <h> OP_EQUAL output by pushing an
// n-byte preimage: no signature, so the cost is hashing.
func sha256PreimageTx(n int) (*tx, error) {
	preimage := randomBytes(n, 1)
	h := sha256.Sum256(preimage)
	var lock bytes.Buffer
	lock.WriteByte(0xa8) // OP_SHA256
	writePush(&lock, h[:])
	lock.WriteByte(0x87) // OP_EQUAL
	var scriptSig bytes.Buffer
	writePush(&scriptSig, preimage)
	return &tx{
		version: txVersion,
		ins: []txIn{{outpoint: outpoint(0), scriptSig: scriptSig.Bytes(), lockingCode: lock.Bytes(),
			satoshis: inputSats, sequence: sequence, utxoHeight: utxoHeight}},
		outs:     []txOut{{outputSats, p2pkhLockingScript(nil)}},
		lockTime: txLockTime,
	}, nil
}

func signedScriptSig(digest []byte, sigHashType byte) ([]byte, error) {
	privKey := make([]byte, 32)
	privKey[31] = 1
	sig, err := secp256k1.SignMessage(digest, privKey)
	if err != nil {
		return nil, err
	}
	var s bytes.Buffer
	writePush(&s, append(sig, sigHashType))
	writePush(&s, mustHex(pubKeyHex))
	return s.Bytes(), nil
}

// randomBytes is deterministic: the same n and seed give the same bytes.
func randomBytes(n int, seed uint64) []byte {
	r := rand.New(rand.NewPCG(seed, 0x62646b))
	b := make([]byte, n)
	for i := range b {
		b[i] = byte(r.Uint32())
	}
	return b
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

// writePush writes a minimal script push of data.
func writePush(w *bytes.Buffer, data []byte) {
	n := len(data)
	switch {
	case n < 0x4c:
		w.WriteByte(byte(n))
	case n <= 0xff:
		w.Write([]byte{0x4c, byte(n)})
	case n <= 0xffff:
		w.WriteByte(0x4d)
		_ = binary.Write(w, binary.LittleEndian, uint16(n))
	default:
		w.WriteByte(0x4e)
		_ = binary.Write(w, binary.LittleEndian, uint32(n))
	}
	w.Write(data)
}

func mustHex(s string) []byte {
	b, err := hex.DecodeString(s)
	if err != nil {
		panic(err)
	}
	return b
}
