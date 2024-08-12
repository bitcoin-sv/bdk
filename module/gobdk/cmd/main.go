package main

import (
	"fmt"
	"github.com/bitcoin-sv/bscrypt/module/gobdk"
)


// main code
func main() {
    fmt.Println("Golang module version : ",gobdk.VersionString())
}