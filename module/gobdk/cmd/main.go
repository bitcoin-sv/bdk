package main

import (
	"fmt"

	"github.com/bitcoin-sv/bdk/module/gobdk"
	"github.com/bitcoin-sv/bdk/module/gobdk/config"
	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// main code
func main() {

	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	asmStrRecover := goscript.ToASM(script)
	fmt.Println("ASM Str         : ", asmStr)
	fmt.Println("ASM Str Recover : ", asmStrRecover)

	fmt.Println("Golang module version : ", gobdk.VersionString())

	settings := config.LoadSetting(
		config.LoadScriptConfig(),
	)
	fmt.Println("Result executing script : ", settings)

	fakeScript := []byte{1, 2, 3, 4, 5}
	e := goscript.ExecuteNoVerify(fakeScript, true, uint(1))
	fmt.Println("Result executing script : ", e)

	// fakeUScript := []byte{1}
	// fakeLScript := []byte{2}
	// fakeTx := []byte{3}

	// v := goscript.Verify(fakeUScript, fakeLScript, true, uint(1), fakeTx, 2, uint64(3))
	// fmt.Println("Result verifying script : ", v)
}
