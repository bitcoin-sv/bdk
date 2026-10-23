//go:build bdk67

package main

import "github.com/bitcoin-sv/bdk/module/gobdk/script"

const haveHelpers = true

func setHelpers(n int64) { script.SetParallelInputHelpers(n) }

func getHelpers() int64 { return script.GetParallelInputHelpers() }
