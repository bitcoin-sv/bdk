package main

import (
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
	fmt.Println("Result executing script : ", settings)

	fakeScript := []byte{1, 2, 3, 4, 5}
	e := goscript.ExecuteNoVerify(fakeScript, true, uint(1))
	fmt.Println("Result executing script : ", e)

	fakeUScript := []byte{1}
	fakeLScript := []byte{2}
	fakeTx := []byte{3}

	v := goscript.Verify(fakeUScript, fakeLScript, true, uint(1), fakeTx, 2, uint64(3))
	fmt.Println("Result verifying script : ", v)
}
