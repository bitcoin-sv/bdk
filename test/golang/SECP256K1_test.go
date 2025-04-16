package main

import (
	"encoding/hex"
	"testing"

	gosecp256k1 "github.com/bitcoin-sv/bdk/module/gobdk/secp256k1"
	"github.com/stretchr/testify/assert"
)

var (
	msg, _ = hex.DecodeString("269d204413554cf4099df30554c8060ecc5f28302252167e6cc6c563c28dad7f")

	// known signature
	sig, _    = hex.DecodeString("304402206BA39DD04FCDDF34CA26F79FDD82E6238A1607BE01EB7F64A53CC83C567E46EE022039265C4D4CA4817FECBB42C943BEF51166C63F640DAD0A555A7A23221A894ECB")
	pubkey, _ = hex.DecodeString("0390c85d6d1f222d2780996ca0666c483986e1762fd46be8fe80750285787186fd")

	// SignMessage vars
	privkey, _ = hex.DecodeString("31a84594060e103f5a63eb742bd46cf5f5900d8406e2726dedfc61c7cf43ebad")
	pubkey2, _ = hex.DecodeString("02087602e71a82777a7a9c234b668a1dc942c9a29bf31c931154eb331c21b6f6fd")
)

func TestVerifySignature(t *testing.T) {
	res := gosecp256k1.VerifySignature(msg, sig, pubkey)
	assert.True(t, res, "Error verifying signature")
}

// Run this test continuously to see of there is a memory leak
//func TestVerifySignature_Memory(t *testing.T) {
//	for {
//		TestVerifySignature(t)
//	}
//}

func TestSignMessage(t *testing.T) {
	signature, err := gosecp256k1.SignMessage(msg, privkey)
	assert.NoError(t, err, "Error signing")
	assert.NotNil(t, signature, "Error signing return nil signature")

	res := gosecp256k1.VerifySignature(msg, signature, pubkey2)
	assert.True(t, res, "Error verifying signature")
}

func BenchmarkVerifySignature(b *testing.B) {
	for i := 0; i < b.N; i++ {
		gosecp256k1.VerifySignature(msg, sig, pubkey)
	}
}
