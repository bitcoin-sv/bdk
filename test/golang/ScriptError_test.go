package main

import (
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

func TestScriptError(t *testing.T) {
	for code := goscript.SCRIPT_ERR_OK; code <= goscript.SCRIPT_ERR_CGO_EXCEPTION; code++ {
		scriptErr := goscript.NewScriptError(code)
		sCode := scriptErr.Code()
		assert.Equal(t, sCode, code, "Code input must match the code in error")
		sStr := scriptErr.Error()
		assert.True(t, len(sStr) > 0, "Error string must be non nil")
	}
}
