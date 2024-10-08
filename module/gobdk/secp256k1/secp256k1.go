package secp256k1

// To make this work on local dev
//   Build the full bdk library, make install it to a specific location. Then set the environment variables
//
//     export BDK_INSTALL_ROOT=/path/to/install/directory
//     export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin"
//     export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include/core -I${BDK_INSTALL_ROOT}/include"
//     export LD_LIBRARY_PATH="${BDK_INSTALL_ROOT}/bin:${LD_LIBRARY_PATH}"
//
// To make a build inside docker, the same, i.e
//   - Get the docker images that have all the necessary dependencies for C++ build
//   - Build this bdk libraries, and make install it to a location
//   - Copy the installed files to the release image. To optimize, copy only the neccessary part,
//   - For golang module, copy only the shared library go, and headers in cgo.
// There might be other system shared library that is required for the executable, just copy them
// to the release docker image. Use lld to know which one is missing.

/*
#cgo LDFLAGS: -lsecp256k1
#include <stdlib.h>
#include <secp256k1/include/secp256k1.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

var ctx *C.secp256k1_context

func init() {
	// Create a secp256k1 context
	ctx = C.secp256k1_context_create(C.SECP256K1_CONTEXT_SIGN | C.SECP256K1_CONTEXT_VERIFY)
}

func VerifySignature(message []byte, signature []byte, publicKey []byte) bool {
	// Allocate memory for the message, signature, and public key
	cMessage := C.CBytes(message)
	defer C.free(cMessage)
	cSignature := C.CBytes(signature)
	defer C.free(cSignature)
	cPublicKey := C.CBytes(publicKey)
	defer C.free(cPublicKey)

	// Create a secp256k1 signature object
	var cSig C.secp256k1_ecdsa_signature
	if C.secp256k1_ecdsa_signature_parse_der(ctx, &cSig, (*C.uchar)(cSignature), C.size_t(len(signature))) != 1 {
		return false
	}

	// Create a secp256k1 public key object
	var cPubKey C.secp256k1_pubkey
	if C.secp256k1_ec_pubkey_parse(ctx, &cPubKey, (*C.uchar)(cPublicKey), C.size_t(len(publicKey))) != 1 {
		return false
	}

	// TODO - check if this is allowed
	// the signatures are normalized before verification - which means the malleability checks are not performed
	// From secp256k1.h: To avoid accepting malleable signatures, only ECDSA signatures in lower-S form are accepted.
	var normalizedCSig C.secp256k1_ecdsa_signature
	C.secp256k1_ecdsa_signature_normalize(ctx, &normalizedCSig, &cSig)

	// Verify the signature
	if C.secp256k1_ecdsa_verify(ctx, &normalizedCSig, (*C.uchar)(cMessage), &cPubKey) != 1 {
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

	// Allocate memory for the message, signature, and public key
	cMessage := C.CBytes(message)
	defer C.free(cMessage)
	cPrivateKey := C.CBytes(privateKey)
	defer C.free(cPrivateKey)

	// Create a secp256k1 signature object
	var cSig C.secp256k1_ecdsa_signature
	result := int(C.secp256k1_ecdsa_sign(ctx, &cSig, (*C.uchar)(cMessage), (*C.uchar)(cPrivateKey), nil, nil))

	if result != 1 {
		return nil, fmt.Errorf("error signing message: %d", result)
	}

	serializedSig := make([]C.uchar, 72)
	outputLen := C.size_t(len(serializedSig))

	result = int(C.secp256k1_ecdsa_signature_serialize_der(ctx, &serializedSig[0], &outputLen, &cSig))

	if result != 1 {
		return nil, fmt.Errorf("error serializing signature: %d", result)
	}

	return C.GoBytes(unsafe.Pointer(&serializedSig[0]), C.int(outputLen)), nil
}
