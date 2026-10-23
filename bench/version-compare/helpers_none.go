//go:build !bdk67

package main

// This gobdk has no parallel input verification.
const haveHelpers = false

func setHelpers(int64) {}

func getHelpers() int64 { return 0 }
