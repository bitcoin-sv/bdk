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
var cmdVerifyBatchSize = 0

// cmdVerify represents the test command
var cmdVerify = &cobra.Command{
	Use:   "verify",
	Short: "Run a test based on a file",
	Long:  `Use this command to run tests from a specific file.`,
	Run:   execVerify,
}

func init() {
	// Define the --csv-file flag for the test command
	cmdVerify.Flags().StringVarP(&cmdVerifyFilePath, "csv-file", "f", "", "Path to the test csv file (required)")

	// Make the --csv-file flag required
	if err := cmdVerify.MarkFlagRequired("csv-file"); err != nil {
		panic(err)
	}

	cmdVerify.Flags().BoolVarP(&cmdVerifyCheckHexOnly, "check-only", "t", false, "Check parsing the extended hex only")

	cmdVerify.Flags().IntVarP(&cmdVerifyBatchSize, "batch-size", "b", 0, "Batch size for verification (0 = no batching, default)")

	cmdRoot.AddCommand(cmdVerify)
}

func execVerify(cmd *cobra.Command, args []string) {
	se := bdkscript.NewScriptEngine(network)
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

	startTime := time.Now()
	var nbFailed int

	if cmdVerifyBatchSize > 0 {
		nbFailed = processVerificationBatch(se, csvData)
	} else {
		nbFailed = processVerificationSingle(se, csvData)
	}

	elapsed := time.Since(startTime)
	tps := float64(len(csvData)) / elapsed.Seconds()
	log.Printf("TOTAL processed %8d txs, elapsed : %8.2f s,    GLOBAL TPS %13.2f, FAILED : %v", len(csvData), elapsed.Seconds(), tps, nbFailed)
}

func processVerificationSingle(se *bdkscript.ScriptEngine, csvData []CsvDataRecord) int {
	localStartTime := time.Now()
	localCount := 0
	processedCount := 0
	nbFailed := 0
	var verifyScriptElapsed time.Duration

	for i, record := range csvData {
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

		// Verify script
		verifyStart := time.Now()
		err = se.VerifyScript(record.TxBinExtended, record.DataUTXOHeights, record.BlockHeight, true)
		verifyScriptElapsed += time.Since(verifyStart)
		if err != nil {
			log.Printf("ERROR verifying record at %v, txID : %v, error \n\n%v\n\n", i, record.TXID, err)
			nbFailed += 1
		}

		localCount += 1
		processedCount += 1

		// Log every 1000 tx
		if localCount >= 1000 {
			timeNow := time.Now()
			elapsed := timeNow.Sub(localStartTime)
			tps := float64(localCount) / elapsed.Seconds()
			log.Printf("Processed %6d txs, TPS %13.2f,     Total : %12d txs",
				localCount, tps, processedCount)
			localCount = 0
			localStartTime = timeNow
		}
	}

	log.Printf("VerifyScript Time: %.4f seconds", verifyScriptElapsed.Seconds())
	return nbFailed
}

func processVerificationBatch(se *bdkscript.ScriptEngine, csvData []CsvDataRecord) int {
	batch := bdkscript.NewVerifyBatch(cmdVerifyBatchSize)
	if batch == nil {
		log.Fatalf("ERROR unable to create verify batch")
	}

	localStartTime := time.Now()
	localCount := 0
	localBatchCount := 0
	processedCount := 0
	processedBatchCount := 0
	nbFailed := 0
	batchIndices := make([]int, 0, cmdVerifyBatchSize)
	var verifyScriptBatchElapsed time.Duration

	for i, record := range csvData {
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

		// Add to batch
		batch.Add(record.TxBinExtended, record.DataUTXOHeights, record.BlockHeight, true, nil)
		batchIndices = append(batchIndices, i)

		// Process batch when full or at end of data
		if batch.Size() >= cmdVerifyBatchSize || i == len(csvData)-1 {
			// Verify batch
			batchSize := batch.Size()
			verifyBatchStart := time.Now()
			results := se.VerifyScriptBatch(batch)
			verifyScriptBatchElapsed += time.Since(verifyBatchStart)

			// Process results
			for j, err := range results {
				if err != nil {
					recordIdx := batchIndices[j]
					log.Printf("ERROR verifying record at %v, txID : %v, error \n\n%v\n\n", recordIdx, csvData[recordIdx].TXID, err)
					nbFailed += 1
				}
			}

			// Update counters
			localCount += batchSize
			localBatchCount += 1
			processedCount += batchSize
			processedBatchCount += 1

			// Log progress every 1000 transactions or at the end
			if localCount >= 1000 || i == len(csvData)-1 {
				timeNow := time.Now()
				elapsed := timeNow.Sub(localStartTime)
				tps := float64(localCount) / elapsed.Seconds()
				log.Printf("Processed %6d txs in %4d batches, TPS %13.2f,     Total : %12d txs in %6d batches",
					localCount, localBatchCount, tps, processedCount, processedBatchCount)
				localCount = 0
				localBatchCount = 0
				localStartTime = timeNow
			}

			// Clear batch for reuse
			batch.Clear()
			batchIndices = batchIndices[:0]
		}
	}

	log.Printf("VerifyScriptBatch Time: %.4f seconds", verifyScriptBatchElapsed.Seconds())
	return nbFailed
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
