//go:build !cgo || bdk_purego

package secp256k1

import (
	"fmt"
	"unsafe"

	"github.com/bitcoin-sv/bdk/module/gobdk/bdkpurego"
	"github.com/ebitengine/purego"
)

// secp256k1 flag constants (from secp256k1.h #defines)
const (
	flagsTypeContext        = (1 << 0)
	flagsBitContextVerify   = (1 << 8)
	flagsBitContextSign     = (1 << 9)
	contextVerify           = flagsTypeContext | flagsBitContextVerify
	contextSign             = flagsTypeContext | flagsBitContextSign
)

var (
	pSecp256k1ContextCreate          func(flags uint32) uintptr
	pSecp256k1ContextDestroy         func(ctx uintptr)
	pSecp256k1EcdsaSignatureParseDer func(ctx uintptr, sig uintptr, input uintptr, inputlen uintptr) int32
	pSecp256k1EcdsaSignatureSerializeDer func(ctx uintptr, output uintptr, outputlen uintptr, sig uintptr) int32
	pSecp256k1EcdsaSignatureNormalize func(ctx uintptr, sigout uintptr, sigin uintptr) int32
	pSecp256k1EcdsaVerify            func(ctx uintptr, sig uintptr, msghash32 uintptr, pubkey uintptr) int32
	pSecp256k1EcdsaSign              func(ctx uintptr, sig uintptr, msghash32 uintptr, seckey uintptr, noncefp uintptr, ndata uintptr) int32
	pSecp256k1EcPubkeyParse          func(ctx uintptr, pubkey uintptr, input uintptr, inputlen uintptr) int32
	pSecp256k1EcPubkeySerialize      func(ctx uintptr, output uintptr, outputlen uintptr, pubkey uintptr, flags uint32) int32
)

var ctx uintptr

func init() {
	if err := bdkpurego.Init(); err != nil {
		panic(err)
	}
	lib := bdkpurego.Handle()

	purego.RegisterLibFunc(&pSecp256k1ContextCreate, lib, "secp256k1_context_create")
	purego.RegisterLibFunc(&pSecp256k1ContextDestroy, lib, "secp256k1_context_destroy")
	purego.RegisterLibFunc(&pSecp256k1EcdsaSignatureParseDer, lib, "secp256k1_ecdsa_signature_parse_der")
	purego.RegisterLibFunc(&pSecp256k1EcdsaSignatureSerializeDer, lib, "secp256k1_ecdsa_signature_serialize_der")
	purego.RegisterLibFunc(&pSecp256k1EcdsaSignatureNormalize, lib, "secp256k1_ecdsa_signature_normalize")
	purego.RegisterLibFunc(&pSecp256k1EcdsaVerify, lib, "secp256k1_ecdsa_verify")
	purego.RegisterLibFunc(&pSecp256k1EcdsaSign, lib, "secp256k1_ecdsa_sign")
	purego.RegisterLibFunc(&pSecp256k1EcPubkeyParse, lib, "secp256k1_ec_pubkey_parse")
	purego.RegisterLibFunc(&pSecp256k1EcPubkeySerialize, lib, "secp256k1_ec_pubkey_serialize")

	ctx = pSecp256k1ContextCreate(contextSign | contextVerify)
}

func VerifySignature(message []byte, signature []byte, publicKey []byte) bool {
	// Parse DER signature into internal format
	var cSig [64]byte
	if pSecp256k1EcdsaSignatureParseDer(ctx, uintptr(unsafe.Pointer(&cSig[0])), uintptr(unsafe.Pointer(&signature[0])), uintptr(len(signature))) != 1 {
		return false
	}

	// Parse public key
	var cPubKey [64]byte
	if pSecp256k1EcPubkeyParse(ctx, uintptr(unsafe.Pointer(&cPubKey[0])), uintptr(unsafe.Pointer(&publicKey[0])), uintptr(len(publicKey))) != 1 {
		return false
	}

	// Normalize the signature (lower-S form)
	var normalizedSig [64]byte
	pSecp256k1EcdsaSignatureNormalize(ctx, uintptr(unsafe.Pointer(&normalizedSig[0])), uintptr(unsafe.Pointer(&cSig[0])))

	// Verify
	if pSecp256k1EcdsaVerify(ctx, uintptr(unsafe.Pointer(&normalizedSig[0])), uintptr(unsafe.Pointer(&message[0])), uintptr(unsafe.Pointer(&cPubKey[0]))) != 1 {
		return false
	}

	return true
}

func SignMessage(message []byte, privateKey []byte) ([]byte, error) {
	if len(message) != 32 {
		return nil, fmt.Errorf("message must be 32 bytes")
	}
	if len(privateKey) != 32 {
		return nil, fmt.Errorf("private key must be 32 bytes")
	}

	// Sign: pass 0 for noncefp (NULL) and 0 for ndata (NULL) to use default nonce function
	var cSig [64]byte
	result := pSecp256k1EcdsaSign(ctx,
		uintptr(unsafe.Pointer(&cSig[0])),
		uintptr(unsafe.Pointer(&message[0])),
		uintptr(unsafe.Pointer(&privateKey[0])),
		0, 0)

	if result != 1 {
		return nil, fmt.Errorf("error signing message: %d", result)
	}

	// Serialize to DER
	serializedSig := make([]byte, 72)
	outputLen := uintptr(len(serializedSig))

	result = pSecp256k1EcdsaSignatureSerializeDer(ctx,
		uintptr(unsafe.Pointer(&serializedSig[0])),
		uintptr(unsafe.Pointer(&outputLen)),
		uintptr(unsafe.Pointer(&cSig[0])))

	if result != 1 {
		return nil, fmt.Errorf("error serializing signature: %d", result)
	}

	return serializedSig[:outputLen], nil
}
