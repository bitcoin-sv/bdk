package main

import (
	"encoding/hex"
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
	"github.com/stretchr/testify/assert"
)

func TestNewScriptEngine(t *testing.T) {
	t.Run("mainnet", func(t *testing.T) {
		se := goscript.NewScriptEngine("main")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(620538))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(921788))
	})

	t.Run("testnet", func(t *testing.T) {
		se := goscript.NewScriptEngine("test")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(1344302))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(1686611))
	})

	t.Run("regtest", func(t *testing.T) {
		se := goscript.NewScriptEngine("regtest")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(10000))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(15000))
	})

	t.Run("stn", func(t *testing.T) {
		se := goscript.NewScriptEngine("stn")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(100))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(250))
	})

	t.Run("teratestnet", func(t *testing.T) {
		se := goscript.NewScriptEngine("teratestnet")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(1))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(1))
	})

	t.Run("tera scaling testnet", func(t *testing.T) {
		se := goscript.NewScriptEngine("tstn")
		assert.NotNil(t, se, "Expect non nil script engine")
		assert.Equal(t, se.GetGenesisActivationHeight(), int32(1))
		assert.Equal(t, se.GetChronicleActivationHeight(), int32(1))
	})

	t.Run("wrong network", func(t *testing.T) {
		se := goscript.NewScriptEngine("foo")
		assert.Nil(t, se, "Expect nil script engine")
	})
}

func TestScriptEnginePolicySettings(t *testing.T) {
	se := goscript.NewScriptEngine("main")

	t.Run("MaxOpsPerScriptPolicy", func(t *testing.T) {
		maxOpsPerScriptPolicyIn := int64(1000)
		err := se.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn)
		assert.Nil(t, err, "SetMaxOpsPerScriptPolicy should be OK")

		maxOpsPerScript := int64(se.GetMaxOpsPerScript(true, false))
		assert.Equal(t, maxOpsPerScriptPolicyIn, maxOpsPerScript)
	})

	t.Run("MaxScriptNumLengthPolicy", func(t *testing.T) {
		maxMaxScriptNumLengthPolicyIn := int64(1000)
		err := se.SetMaxScriptNumLengthPolicy(maxMaxScriptNumLengthPolicyIn)
		assert.Nil(t, err, "SetMaxScriptNumLengthPolicy should be OK")

		maxMaxScriptNumLength := int64(se.GetMaxScriptNumLength(true, false, false))
		assert.Equal(t, maxMaxScriptNumLength, maxMaxScriptNumLengthPolicyIn)
	})

	t.Run("MaxScriptSizePolicy", func(t *testing.T) {
		maxScriptSizePolicyIn := int64(1000)
		err := se.SetMaxScriptSizePolicy(maxScriptSizePolicyIn)
		assert.Nil(t, err, "SetMaxScriptSizePolicy should be OK")

		maxScriptSize := int64(se.GetMaxScriptSize(true, false))
		assert.Equal(t, maxScriptSizePolicyIn, maxScriptSize)
	})

	t.Run("MaxPubKeysPerMultiSigPolicy", func(t *testing.T) {
		maxPubKeysPerMultiSigPolicyIn := int64(1000)
		err := se.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigPolicyIn)
		assert.Nil(t, err, "SetMaxPubKeysPerMultiSigPolicy should be OK")

		maxPubKeysPerMultiSig := int64(se.GetMaxPubKeysPerMultiSig(true, false))
		assert.Equal(t, maxPubKeysPerMultiSigPolicyIn, maxPubKeysPerMultiSig)
	})

	t.Run("MaxStackMemoryUsage", func(t *testing.T) {
		maxStackMemoryUsageConsensusIn := int64(2000)
		maxStackMemoryUsageIn := int64(1000)
		err := se.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsageIn)
		assert.Nil(t, err, "SetMaxStackMemoryUsage should be OK")

		maxStackMemoryUsage := int64(se.GetMaxStackMemoryUsage(true, false))
		assert.Equal(t, maxStackMemoryUsageIn, maxStackMemoryUsage)
	})

	t.Run("GenesisActivationHeight", func(t *testing.T) {
		genesisActivationHeightIn := int32(1000)
		err := se.SetGenesisActivationHeight(genesisActivationHeightIn)
		assert.Nil(t, err, "GenesisActivationHeight should be OK")

		genesisActivationHeight := int32(se.GetGenesisActivationHeight())
		assert.Equal(t, genesisActivationHeightIn, genesisActivationHeight)
	})

	t.Run("ChronicleActivationHeight", func(t *testing.T) {
		chronicleActivationHeightIn := int32(1000)
		err := se.SetChronicleActivationHeight(chronicleActivationHeightIn)
		assert.Nil(t, err, "ChronicleActivationHeight should be OK")

		chronicleActivationHeight := int32(se.GetChronicleActivationHeight())
		assert.Equal(t, chronicleActivationHeightIn, chronicleActivationHeight)
	})
}

func TestScriptEngineGetSigOpCount(t *testing.T) {
	t.Run("TXID 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105", func(t *testing.T) {
		//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
		eTxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
		utxo := []int32{631924, 631924}
		blockHeight := int32(632099)

		eTx, _ := hex.DecodeString(eTxHEX)

		se := goscript.NewScriptEngine("main")
		nbSigOpos, err := se.GetSigOpCount(eTx, utxo, blockHeight)
		assert.Nil(t, err, "GetSigOpCount should return no error")
		assert.Equal(t, nbSigOpos, uint64(1), "GetSigOpCount should return no zero value")
	})
}

func TestScriptEngineScriptVerify(t *testing.T) {
	t.Run("TXID 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105", func(t *testing.T) {
		//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
		eTxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
		utxo := []int32{631924, 631924}
		blockHeight := int32(632099)

		eTx, _ := hex.DecodeString(eTxHEX)

		se := goscript.NewScriptEngine("main")
		err := se.VerifyScript(eTx, utxo, blockHeight, true)
		assert.Nil(t, err, "VerifyExtend should return no error")
	})

	t.Run("Empty UTXO heights", func(t *testing.T) {
		//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
		originETxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
		utxo := []int32{}
		blockHeight := int32(632099)

		tx, errTx := bt.NewTxFromString(originETxHEX)
		assert.Nil(t, errTx, "Failed to deserialize the transaction")

		// Empty the utxo list
		tx.Inputs = tx.Inputs[:0]
		eTxHEX := hex.EncodeToString(tx.ExtendedBytes())
		eTx, _ := hex.DecodeString(eTxHEX)

		// VerifyExtendFull the transaction with empty inputs, expect error
		se := goscript.NewScriptEngine("main")
		err := se.VerifyScript(eTx, utxo, blockHeight, true)
		assert.NotNil(t, err, "VerifyScript should return error for zero utxo")
	})
}

func TestScriptEngineCheckConsensus(t *testing.T) {
	t.Run("TXID 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105", func(t *testing.T) {
		//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
		eTxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
		utxo := []int32{631924, 631924}
		blockHeight := int32(632099)

		eTx, _ := hex.DecodeString(eTxHEX)

		se := goscript.NewScriptEngine("main")
		err := se.CheckConsensus(eTx, utxo, blockHeight)
		assert.Nil(t, err, "CheckConsensus should return no error")
	})
}

func TestScriptEngineScriptVerifyWithCustomFlags(t *testing.T) {
	t.Run("TXID 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105", func(t *testing.T) {
		//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
		eTxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
		utxo := []int32{631924, 631924}
		flags := []uint32{869935, 869935}
		blockHeight := int32(632099)

		eTx, _ := hex.DecodeString(eTxHEX)

		se := goscript.NewScriptEngine("main")
		err := se.VerifyScriptWithCustomFlags(eTx, utxo, blockHeight, true, flags)
		assert.Nil(t, err, "WithCustomFlags should return no error")
	})
}
