package main

import (
	"encoding/hex"
	"fmt"

	"github.com/bitcoin-sv/bdk/module/gobdk"
	"github.com/bitcoin-sv/bdk/module/gobdk/config"
	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// main code
func main() {

	fmt.Println("Golang module version : ", gobdk.VersionString())

	settings := config.LoadSetting(
		config.LoadScriptConfig(),
	)
	fmt.Println("Setting : ", settings)

	// Test from/to ASM
	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	asmStrRecover := goscript.ToASM(script)
	fmt.Println("ASM Str         : ", asmStr)
	fmt.Println("ASM Str Recover : ", asmStrRecover)
	e := goscript.ExecuteNoVerify(script, true, uint(0))
	fmt.Println("Result executing script : ", e)

	// Test verify
	uScriptStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	lScriptStr := "DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIG"
	txHex := "0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000"

	uScript := goscript.FromASM(uScriptStr)
	lScript := goscript.FromASM(lScriptStr)
	tx, _ := hex.DecodeString(txHex)

	v := goscript.Verify(uScript, lScript, true, uint(0), tx, int(0), uint64(10))
	fmt.Println("Result verifying script : ", v)
}
