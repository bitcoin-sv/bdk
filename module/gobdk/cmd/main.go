package main

import (
	"fmt"

	"github.com/bitcoin-sv/bscrypt/module/gobdk/config"
)

// main code
func main() {

	//fmt.Println("Golang module version : ", gobdk.VersionString())

	settings := config.LoadSetting(
		config.LoadScriptConfig(),
	)
	fmt.Println("Result executing script : ", settings)

	// fakeScript := []byte{1, 2, 3, 4, 5}
	// e := gobdk.Execute(fakeScript, true, uint(1))
	// fmt.Println("Result executing script : ", e)
}
