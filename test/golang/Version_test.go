package main

import (
	"testing"

	"github.com/bitcoin-sv/bdk/module/gobdk"
	"github.com/stretchr/testify/assert"
)

func TestVersionBSV(t *testing.T) {
	assert.Equal(t, 1, gobdk.BSV_CLIENT_VERSION_MAJOR(), "BSV Version Major")
	assert.Equal(t, 2, gobdk.BSV_CLIENT_VERSION_MINOR(), "BSV Version Minor")
	assert.Equal(t, 0, gobdk.BSV_CLIENT_VERSION_REVISION(), "BSV Version Patch")
}

func TestVersionBDK(t *testing.T) {
	assert.Equal(t, 1, gobdk.BDK_VERSION_MAJOR(), "BDK Version Major")
	assert.Equal(t, 2, gobdk.BDK_VERSION_MINOR(), "BDK Version Minor")
	assert.Equal(t, 0, gobdk.BDK_VERSION_PATCH(), "BDK Version Patch")
}

func TestVersionGoBDK(t *testing.T) {
	assert.Equal(t, 1, gobdk.BDK_GOLANG_VERSION_MAJOR(), "GoBDK Version Major")
	assert.Equal(t, 2, gobdk.BDK_GOLANG_VERSION_MINOR(), "GoBDK Version Minor")
	assert.Equal(t, 2, gobdk.BDK_GOLANG_VERSION_PATCH(), "GoBDK Version Patch")
}
