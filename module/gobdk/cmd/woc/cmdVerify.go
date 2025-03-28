package main

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"time"

	bdkconfig "github.com/bitcoin-sv/bdk/module/gobdk/config"
	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
	"github.com/spf13/cobra"
)

var cmdVerifyFilePath string
var cmdVerifyCheckHexOnly = false
var cmdVerifyVerifyExtend = true

// cmdVerify represents the test command
var cmdVerify = &cobra.Command{
	Use:   "verify",
	Short: "Run a test based on a file",
	Long:  `Use this command to run tests from a specific file.`,
	Run:   execVerify,
}

func init() {
	// Define the --file-path flag for the test command
	cmdVerify.Flags().StringVarP(&cmdVerifyFilePath, "file-path", "f", "", "Path to the test file (required)")

	// Make the --file-path flag required
	if err := cmdVerify.MarkFlagRequired("file-path"); err != nil {
		panic(err)
	}

	cmdVerify.Flags().BoolVarP(&cmdVerifyCheckHexOnly, "check-only", "t", false, "Check parsing the extended hex only")
	cmdVerify.Flags().BoolVarP(&cmdVerifyVerifyExtend, "verify-extend", "e", true, "Run VerifyExtend instead of verify")

	cmdRoot.AddCommand(cmdVerify)
}

func setGlobalScriptConfig(network string) error {
	bdkScriptConfig := bdkconfig.ScriptConfig{
		ChainNetwork: network,
		// MaxOpsPerScriptPolicy:        uint64(po.MaxOpsPerScriptPolicy),
		// MaxScriptNumLengthPolicy:     uint64(po.MaxScriptNumLengthPolicy),
		// MaxScriptSizePolicy:          uint64(po.MaxScriptSizePolicy),
		// MaxPubKeysPerMultiSig:        uint64(po.MaxPubKeysPerMultisigPolicy),
		// MaxStackMemoryUsageConsensus: uint64(po.MaxStackMemoryUsageConsensus),
		// MaxStackMemoryUsagePolicy:    uint64(po.MaxStackMemoryUsagePolicy),
	}

	return bdkscript.SetGlobalScriptConfig(bdkScriptConfig)
}

func execVerify(cmd *cobra.Command, args []string) {

	if err := setGlobalScriptConfig(network); err != nil {
		log.Fatalf("ERROR while setting global script config with network %v, error \n\n%v\n\n", network, err)
	}

	csvData, err := ReadCSVFile(cmdVerifyFilePath)
	if err != nil {
		log.Fatal(err)
	}

	// Check the network is consistent
	for i := 0; i < len(csvData); i++ {
		if csvData[i].ChainNet != network {
			log.Fatalf("ERROR record %v, inconsistent network. Data %v, input %v", i, csvData[i].ChainNet, network)
		}
	}

	globalStartTime, localStartTime := time.Now(), time.Now()
	localCount, globalCount := -1, -1
	nbFailed := 0
	for i, record := range csvData {
		localCount += 1
		globalCount += 1

		// Log every 1000 tx
		if i%1000 == 1 {
			timeNow := time.Now()
			elapsed := timeNow.Sub(localStartTime)
			tps := float64(localCount) / elapsed.Seconds()
			log.Printf("Processed %6d txs, TPS %13.2f,     Total : %12d txs, elapsed : %8.2f s", localCount, tps, globalCount, timeNow.Sub(globalStartTime).Seconds())
			localCount = 0
			localStartTime = timeNow
		}

		tx, err := bt.NewTxFromString(record.TxHexExtended)
		if err != nil || tx == nil {
			log.Printf("ERROR parsing extended tx at record %v, txID : %v, error \n\n%v\n\n", i, record.TXID, err)
			continue
		}

		// Test only the hex is conformed
		if cmdVerifyCheckHexOnly {
			txHexExtended := hex.EncodeToString(tx.ExtendedBytes())
			if txHexExtended != record.TxHexExtended {
				log.Printf("ERROR recovering extended tx at record %v, txID : %v", i, record.TXID)
			}
			continue
		}

		if cmdVerifyVerifyExtend {
			if err := verifyScriptExtendFull(record.TxBinExtended, record.DataUTXOHeights, uint32(record.BlockHeight)); err != nil {
				log.Printf("ERROR verifying record at %v, txID : %v, error \n\n%v\n\n", i, record.TXID, err)
				nbFailed += 1
			}
		} else {
			//Test the script veriry
			if err := verifyScript(tx, uint32(record.BlockHeight)); err != nil {
				log.Printf("ERROR verifying record at %v, txID : %v, error \n\n%v\n\n", i, record.TXID, err)
				nbFailed += 1
			}
		}
	}

	elapsed := time.Since(globalStartTime)
	tps := float64(globalCount) / elapsed.Seconds()
	log.Printf("TOTAL processed %8d txs, elapsed : %8.2f s,    GLOBAL TPS %13.2f, FAILED : %v", globalCount, elapsed.Seconds(), tps, nbFailed)
}

// //////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////

// Test new version of verificator
func verifyScriptExtendFull(txBin []byte, utxoHeights []uint32, blockHeight uint32) error {
	if errV := bdkscript.VerifyExtendFull(txBin, utxoHeights, blockHeight-1, true); errV != nil {
		return errV
	}

	return nil
}

// Copied exactly the verify in ubsv
func verifyScript(tx *bt.Tx, blockHeight uint32) error {

	for i, in := range tx.Inputs {
		if in.PreviousTxScript == nil || in.UnlockingScript == nil {
			continue
		}

		// TODO : For now, as there are only one way to pass go []byte to C++ array and assume
		// the array is not empty, we actually cannot pass empty []byte to C++ array
		// In future, we must handle this case where empty array is still valid and need to verify
		// See https://github.com/bitcoin-sv/ubsv/issues/1270
		if len(*in.PreviousTxScript) < 1 || len(*in.UnlockingScript) < 1 {
			continue
		}

		// isPostChronicle now is unknow, in future, it need to be calculate based on block height
		//flags, errF := bdkscript.ScriptVerificationFlagsV1(*in.PreviousTxScript, false)
		flags, errF := bdkscript.ScriptVerificationFlagsV2(*in.PreviousTxScript, blockHeight-1)
		if errF != nil {
			return fmt.Errorf("failed to calculate flags from prev locking script, flags : %v, error: %v", flags, errF)
		}

		if errV := bdkscript.Verify(*in.UnlockingScript, *in.PreviousTxScript, true, uint(flags), tx.Bytes(), i, in.PreviousTxSatoshis); errV != nil {
			// Helpful logging to get full information to debug separately in GoBDK

			errLog := bdkDebugVerification{
				UScript:     hex.EncodeToString(*in.UnlockingScript),
				LScript:     hex.EncodeToString(*in.PreviousTxScript),
				TxBytes:     tx.String(),
				Flags:       flags,
				Input:       i,
				Satoshis:    in.PreviousTxSatoshis,
				BlockHeight: blockHeight,
				Err:         errV.Error(),
			}

			errLogData, _ := json.MarshalIndent(errLog, "", "    ")

			errorLogMsg := fmt.Sprintf("Failed to verify script in go-bdk, error : \n\n%v\n\n", string(errLogData))
			return fmt.Errorf("Failed to verify script: %w\n\nerror detail\n\n%v", errV, errorLogMsg)
		}
	}

	return nil
}

type bdkDebugVerification struct {
	UScript     string `mapstructure:"uScript" json:"uScript" validate:"required"`
	LScript     string `mapstructure:"lScript" json:"lScript" validate:"required"`
	TxBytes     string `mapstructure:"txBytes" json:"txBytes" validate:"required"`
	Flags       uint32 `mapstructure:"flags" json:"flags" validate:"required"`
	Input       int    `mapstructure:"input" json:"input" validate:"required"`
	Satoshis    uint64 `mapstructure:"satoshis" json:"satoshis" validate:"required"`
	BlockHeight uint32 `mapstructure:"blockHeight" json:"blockHeight" validate:"required"`
	Err         string `mapstructure:"err" json:"err" validate:"required"`
}
