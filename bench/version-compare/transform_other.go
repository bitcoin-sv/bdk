//go:build !(amd64 && bdk14)

package main

func transformAddr() uint64 { return 0 }
