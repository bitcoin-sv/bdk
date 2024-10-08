package main

import (
	"encoding/hex"
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

func TestASMConversion(t *testing.T) {

	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	asmStrRecover := goscript.ToASM(script)
	assert.Equal(t, asmStr, asmStrRecover, "Recovered from conversion should match initial script string")
}

func TestScriptExecutionNoVerify(t *testing.T) {

	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	err := goscript.ExecuteNoVerify(script, true, uint(0))
	assert.Nil(t, err, "Execution script should return no error")
}

func TestScriptVerify(t *testing.T) {

	uScriptHex := "483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7f"
	lScriptHex := "76a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac"
	txHex := "01000000012341c8f267c6a1a407b1f09d134e42cfbdf2ebc91aff8cd4365d131380fcd580000000006b483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7fffffffff01a0860100000000001976a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac00000000"

	uScript, _ := hex.DecodeString(uScriptHex)
	lScript, _ := hex.DecodeString(lScriptHex)
	tx, _ := hex.DecodeString(txHex)

	inIndex := 0
	satoshis := uint64(100000)
	blockHeight := uint32(620538)
	//flags, _ := goscript.ScriptVerificationFlags(lScript, true)
	flags, _ := goscript.ScriptVerificationFlagsV2(lScript, blockHeight)

	err := goscript.Verify(uScript, lScript, true, uint(flags), tx, inIndex, satoshis)
	assert.Nil(t, err, "Verify script should return no error")
}
