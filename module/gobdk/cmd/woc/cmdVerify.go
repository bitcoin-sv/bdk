package main

import (
	"encoding/hex"
	"log"
	"time"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/libsv/go-bt/v2"
	"github.com/spf13/cobra"
)

var cmdVerifyFilePath string
var cmdVerifyCheckHexOnly = false

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

	cmdRoot.AddCommand(cmdVerify)
}

func execVerify(cmd *cobra.Command, args []string) {

	se := bdkscript.NewScriptEngine("main")
	if se == nil {
		log.Fatalf("ERROR unable to create script engine")
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

		if err := se.VerifyScript(record.TxBinExtended, record.DataUTXOHeights, record.BlockHeight-1, true); err != nil {
			log.Printf("ERROR verifying record at %v, txID : %v, error \n\n%v\n\n", i, record.TXID, err)
			nbFailed += 1
		}
	}

	elapsed := time.Since(globalStartTime)
	tps := float64(globalCount) / elapsed.Seconds()
	log.Printf("TOTAL processed %8d txs, elapsed : %8.2f s,    GLOBAL TPS %13.2f, FAILED : %v", globalCount, elapsed.Seconds(), tps, nbFailed)
}

// //////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////

type bdkDebugVerification struct {
	UScript     string  `mapstructure:"uScript" json:"uScript" validate:"required"`
	LScript     string  `mapstructure:"lScript" json:"lScript" validate:"required"`
	TxBytes     string  `mapstructure:"txBytes" json:"txBytes" validate:"required"`
	Flags       uint32  `mapstructure:"flags" json:"flags" validate:"required"`
	Input       int     `mapstructure:"input" json:"input" validate:"required"`
	Satoshis    uint64  `mapstructure:"satoshis" json:"satoshis" validate:"required"`
	BlockHeight int32   `mapstructure:"blockHeight" json:"blockHeight" validate:"required"`
	UTXOHeights []int32 `mapstructure:"utxoHeights" json:"utxoHeights" validate:"required"`
	Err         string  `mapstructure:"err" json:"err" validate:"required"`
}
