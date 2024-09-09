package main

import (
	"testing"

	"github.com/stretchr/testify/assert"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

func TestExceptionFlagsCalculation(t *testing.T) {
	t.Run("Out-of-bound exception", func(t *testing.T) {
		lScript := make([]byte, 22)
		_, err := goscript.ScriptVerificationFlags(lScript, true)
		assert.NotNil(t, err, "Expect exception error")
	})

	t.Run("Correct size locking script", func(t *testing.T) {
		lScript := make([]byte, 23)
		_, err := goscript.ScriptVerificationFlags(lScript, true)
		assert.Nil(t, err, "No exception for correct size of locking script")
	})

	t.Run("Correct size locking script ++", func(t *testing.T) {
		lScript := make([]byte, 24)
		_, err := goscript.ScriptVerificationFlags(lScript, true)
		assert.Nil(t, err, "No exception for correct size of locking script")
	})
}

func TestExceptionBadTransaction(t *testing.T) {
	t.Run("Bad tx serialization", func(t *testing.T) {
		err := goscript.Verify([]byte{0}, []byte{0}, true, 0, []byte{0}, 0, 0)
		assert.NotNil(t, err, "Expect error")
		assert.Equal(t, err.Code(), goscript.SCRIPT_ERR_CGO_EXCEPTION, "Expect exception error")
	})
}
