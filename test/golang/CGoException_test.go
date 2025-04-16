package main

import (
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

func TestExceptionBadTransaction(t *testing.T) {
	t.Run("handle wrong network exception", func(t *testing.T) {
		se := goscript.NewScriptEngine("foo")
		assert.Nil(t, se, "Expect nil script engine")
	})

	t.Run("Bad tx serialization", func(t *testing.T) {
		se := goscript.NewScriptEngine("main")
		err := se.VerifyScript([]byte{0}, []int32{0}, int32(0), true)
		assert.NotNil(t, err, "Expect error")
		assert.Equal(t, err.Code(), goscript.SCRIPT_ERR_CGO_EXCEPTION, "Expect exception error")
	})
}
