//go:build bdk14

package main

import gobdk "github.com/bitcoin-sv/bdk/module/gobdk"

func sha256Backend() string { return gobdk.SHA256_BACKEND() }
