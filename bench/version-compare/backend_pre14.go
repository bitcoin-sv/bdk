//go:build !bdk14

package main

// gobdk before v1.4.0 has no SHA256_BACKEND: it always hashes with the scalar code.
func sha256Backend() string { return "n/a (pre-v1.4.0)" }
