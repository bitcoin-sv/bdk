package main

import (
	"fmt"
	"os"
	"testing"

	bdkconfig "github.com/bitcoin-sv/bdk/module/gobdk/config"
	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

func TestMain(m *testing.M) {
	// Setup code (runs before all tests)
	setup()

	// Run the tests
	code := m.Run()

	// Cleanup code (runs after all tests)
	teardown()

	// Exit with the appropriate code
	os.Exit(code)
}

func setup() {
	bdkScriptConfig := bdkconfig.ScriptConfig{
		ChainNetwork: "main",
	}

	err := bdkscript.SetGlobalScriptConfig(bdkScriptConfig)
	if err != nil {
		panic(fmt.Errorf("unable to setup test, error : %w", err))
	}
}

func teardown() {
}
