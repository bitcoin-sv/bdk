package main

import (
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

func TestExceptionBadTransaction(t *testing.T) {
	t.Run("handle wrong network exception", func(t *testing.T) {
		se := goscript.NewTxValidator("foo")
		assert.Nil(t, se, "Expect nil script engine")
	})

	t.Run("Bad tx serialization", func(t *testing.T) {
		se := goscript.NewTxValidator("main")
		err := se.VerifyScript([]byte{0}, []int32{0}, int32(0), true)
		assert.NotNil(t, err, "Expect error")
		scriptErr, ok := err.(goscript.ScriptError)
		assert.True(t, ok, "Expect ScriptError type")
		assert.Equal(t, goscript.SCRIPT_ERR_CGO_EXCEPTION, scriptErr.Code(), "Expect exception error")
	})
}
