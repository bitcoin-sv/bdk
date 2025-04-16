package main

import (
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

func TestScriptError(t *testing.T) {
	t.Run("Code and String", func(t *testing.T) {
		for code := goscript.SCRIPT_ERR_OK; code <= goscript.SCRIPT_ERR_CGO_EXCEPTION; code++ {
			scriptErr := goscript.NewScriptError(code)
			sCode := scriptErr.Code()
			assert.Equal(t, sCode, code, "Code input must match the code in error")
			sStr := scriptErr.Error()
			assert.True(t, len(sStr) > 0, "Error string must be non nil")
		}
	})

	t.Run("Max error code", func(t *testing.T) {
		goErrBound := int(goscript.SCRIPT_ERR_ERROR_COUNT)
		cppErrBound := goscript.CPP_SCRIPT_ERR_ERROR_COUNT()

		assert.Equal(t, goErrBound, cppErrBound, "ERROR_COUNT must match in go and cpp")
		assert.Equal(t, goscript.SCRIPT_ERR_ERROR_COUNT+1, goscript.SCRIPT_ERR_CGO_EXCEPTION, "SCRIPT_ERR_CGO_EXCEPTION must be SCRIPT_ERR_ERROR_COUNT increment 1")
	})

}
