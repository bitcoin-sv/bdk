package main

import (
	"encoding/hex"
	"testing"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
	"github.com/stretchr/testify/assert"
)

func TestGetGenesisActivationHeight(t *testing.T) {
	mainnetGH := goscript.GetGenesisActivationHeight()
	assert.Equal(t, mainnetGH, uint32(620538), "default genesis height for mainnet should match")
}

func TestASMConversion(t *testing.T) {

	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	asmStrRecover := goscript.ToASM(script)
	assert.Equal(t, asmStr, asmStrRecover, "Recovered from conversion should match initial script string")
}

func TestScriptExecutionNoVerify(t *testing.T) {

	asmStr := "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
	script := goscript.FromASM(asmStr)
	err := goscript.ExecuteNoVerify(script, true, uint(0))
	assert.Nil(t, err, "Execution script should return no error")
}

func TestScriptVerify(t *testing.T) {

	uScriptHex := "483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7f"
	lScriptHex := "76a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac"
	txHex := "01000000012341c8f267c6a1a407b1f09d134e42cfbdf2ebc91aff8cd4365d131380fcd580000000006b483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7fffffffff01a0860100000000001976a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac00000000"

	uScript, _ := hex.DecodeString(uScriptHex)
	lScript, _ := hex.DecodeString(lScriptHex)
	tx, _ := hex.DecodeString(txHex)

	inIndex := 0
	satoshis := uint64(100000)
	blockHeight := uint32(620538)
	//flags, _ := goscript.ScriptVerificationFlagsV1(lScript, true)
	flags, _ := goscript.ScriptVerificationFlagsV2(lScript, blockHeight)

	err := goscript.Verify(uScript, lScript, true, uint(flags), tx, inIndex, satoshis)
	assert.Nil(t, err, "Verify script should return no error")
}

func TestScriptVerifyExtend(t *testing.T) {
	//This is on mainnet TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
	eTxHEX := "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000"
	blockHeight := uint32(108522)

	eTx, _ := hex.DecodeString(eTxHEX)

	err := goscript.VerifyExtend(eTx, blockHeight-1, true)
	assert.Nil(t, err, "VerifyExtend should return no error")
}

func TestScriptVerifyExtendFull(t *testing.T) {
	//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
	eTxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
	utxo := []uint32{631924, 631924}
	blockHeight := uint32(632099)

	eTx, _ := hex.DecodeString(eTxHEX)

	err := goscript.VerifyExtendFull(eTx, utxo, blockHeight-1, true)
	assert.Nil(t, err, "VerifyExtend should return no error")
}

func TestScriptVerifyExtendEmptyUTXO(t *testing.T) {
	//This is on mainnet TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
	originETxHEX := "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000"
	blockHeight := uint32(108522)

	tx, err := bt.NewTxFromString(originETxHEX)
	assert.Nil(t, err, "Failed to deserialize the transaction")

	// Empty the utxo list
	tx.Inputs = tx.Inputs[:0]
	eTxHEX := hex.EncodeToString(tx.ExtendedBytes())
	eTx, _ := hex.DecodeString(eTxHEX)

	// VerifyExtend the transaction with empty inputs, expect error
	errVerify := goscript.VerifyExtend(eTx, blockHeight-1, true)
	assert.NotNil(t, errVerify, "VerifyExtend should return error for zero utxo")
}

func TestScriptVerifyExtendFullEmptyUTXO(t *testing.T) {
	//This is on mainnet TxID = "7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105";
	originETxHEX := "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"
	utxo := []uint32{}
	blockHeight := uint32(632099)

	tx, err := bt.NewTxFromString(originETxHEX)
	assert.Nil(t, err, "Failed to deserialize the transaction")

	// Empty the utxo list
	tx.Inputs = tx.Inputs[:0]
	eTxHEX := hex.EncodeToString(tx.ExtendedBytes())
	eTx, _ := hex.DecodeString(eTxHEX)

	// VerifyExtendFull the transaction with empty inputs, expect error
	errVerify := goscript.VerifyExtendFull(eTx, utxo, blockHeight-1, true)
	assert.NotNil(t, errVerify, "VerifyExtendFull should error for zero utxo")
}
