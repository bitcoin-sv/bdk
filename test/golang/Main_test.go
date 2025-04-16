package main

import (
	"os"
	"testing"
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
	// TODO
	// Add test setup here
}

func teardown() {
	// TODO
	// Add test tears down here
}
