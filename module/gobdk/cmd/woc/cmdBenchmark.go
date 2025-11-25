package main

import (
	"log"
	"time"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/spf13/cobra"
)

var cmdBenchmarkFilePath string
var cmdBenchmarkBatchSize = 0
var cmdBenchmarkIterations = 1

// cmdBenchmark represents the benchmark command
var cmdBenchmark = &cobra.Command{
	Use:   "benchmark",
	Short: "Run benchmark to compare single vs batch verification",
	Long:  `Use this command to benchmark verification performance with multiple iterations.`,
	Run:   execBenchmark,
}

func init() {
	// Define the --csv-file flag for the benchmark command
	cmdBenchmark.Flags().StringVarP(&cmdBenchmarkFilePath, "csv-file", "f", "", "Path to the test csv file (required)")

	// Make the --csv-file flag required
	if err := cmdBenchmark.MarkFlagRequired("csv-file"); err != nil {
		panic(err)
	}

	cmdBenchmark.Flags().IntVarP(&cmdBenchmarkBatchSize, "batch-size", "b", 100, "Batch size for verification (default: 100)")

	cmdBenchmark.Flags().IntVarP(&cmdBenchmarkIterations, "iterations", "i", 1, "Number of iterations to run (default: 1)")

	cmdRoot.AddCommand(cmdBenchmark)
}

func execBenchmark(cmd *cobra.Command, args []string) {
	se := bdkscript.NewScriptEngine(network)
	if se == nil {
		log.Fatalf("ERROR unable to create script engine")
	}

	// Load CSV data once
	log.Printf("Loading CSV data from %s...", cmdBenchmarkFilePath)
	csvData, err := ReadCSVFile(cmdBenchmarkFilePath)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("Loaded %d records", len(csvData))

	// Check the network is consistent
	for i := 0; i < len(csvData); i++ {
		if csvData[i].ChainNet != network {
			log.Fatalf("ERROR record %v, inconsistent network. Data %v, input %v", i, csvData[i].ChainNet, network)
		}
	}

	var totalSingleTime time.Duration
	var totalBatchTime time.Duration
	var totalBatchAddTime time.Duration
	overallStart := time.Now()

	log.Printf("Starting benchmark with %d iterations, batch size %d", cmdBenchmarkIterations, cmdBenchmarkBatchSize)

	for iter := 1; iter <= cmdBenchmarkIterations; iter++ {
		iterStart := time.Now()

		// Run single verification
		singleTime := runSingleVerification(se, csvData)
		totalSingleTime += singleTime

		// Run batch verification
		batchTime, batchAddTime := runBatchVerification(se, csvData, cmdBenchmarkBatchSize)
		totalBatchTime += batchTime
		totalBatchAddTime += batchAddTime

		iterElapsed := time.Since(iterStart)
		log.Printf("Iteration %d/%d completed in %.4f seconds (Single: %.4f s, Batch: %.4f s, BatchAdd: %.4f s)",
			iter, cmdBenchmarkIterations, iterElapsed.Seconds(), singleTime.Seconds(), batchTime.Seconds(), batchAddTime.Seconds())
	}

	overallElapsed := time.Since(overallStart)

	// Calculate averages
	avgSingleTime := totalSingleTime.Seconds() / float64(cmdBenchmarkIterations)
	avgBatchTime := totalBatchTime.Seconds() / float64(cmdBenchmarkIterations)
	avgBatchAddTime := totalBatchAddTime.Seconds() / float64(cmdBenchmarkIterations)

	// Print summary
	log.Printf("========================================")
	log.Printf("Benchmark Summary")
	log.Printf("========================================")
	log.Printf("Total records:        %d", len(csvData))
	log.Printf("Iterations:           %d", cmdBenchmarkIterations)
	log.Printf("Batch size:           %d", cmdBenchmarkBatchSize)
	log.Printf("Overall processing:   %.4f seconds", overallElapsed.Seconds())
	log.Printf("----------------------------------------")
	log.Printf("Total Single time:    %.4f seconds", totalSingleTime.Seconds())
	log.Printf("Total Batch time:     %.4f seconds", totalBatchTime.Seconds())
	log.Printf("Total BatchAdd time:  %.4f seconds", totalBatchAddTime.Seconds())
	log.Printf("----------------------------------------")
	log.Printf("Average Single time:  %.4f seconds", avgSingleTime)
	log.Printf("Average Batch time:   %.4f seconds", avgBatchTime)
	log.Printf("Average BatchAdd time:%.4f seconds", avgBatchAddTime)
	log.Printf("----------------------------------------")
	if avgBatchTime > 0 {
		speedup := avgSingleTime / avgBatchTime
		log.Printf("Batch speedup:        %.2fx", speedup)
	}
	log.Printf("========================================")
}

func runSingleVerification(se *bdkscript.ScriptEngine, csvData []CsvDataRecord) time.Duration {
	var verifyScriptElapsed time.Duration
	nbFailed := 0

	for _, record := range csvData {
		verifyStart := time.Now()
		err := se.VerifyScript(record.TxBinExtended, record.DataUTXOHeights, record.BlockHeight, true)
		verifyScriptElapsed += time.Since(verifyStart)

		if err != nil {
			nbFailed++
		}
	}

	if nbFailed > 0 {
		log.Printf("Single verification: %d failed out of %d", nbFailed, len(csvData))
	}

	return verifyScriptElapsed
}

func runBatchVerification(se *bdkscript.ScriptEngine, csvData []CsvDataRecord, batchSize int) (time.Duration, time.Duration) {
	batch := bdkscript.NewVerifyBatch(batchSize)
	if batch == nil {
		log.Fatalf("ERROR unable to create verify batch")
	}

	var verifyScriptBatchElapsed time.Duration
	var batchAddElapsed time.Duration
	nbFailed := 0
	batchIndices := make([]int, 0, batchSize)

	for i, record := range csvData {
		// Add to batch
		addStart := time.Now()
		batch.Add(record.TxBinExtended, record.DataUTXOHeights, record.BlockHeight, true, nil)
		batchAddElapsed += time.Since(addStart)
		batchIndices = append(batchIndices, i)

		// Process batch when full or at end of data
		if batch.Size() >= batchSize || i == len(csvData)-1 {
			// Verify batch
			verifyBatchStart := time.Now()
			results := se.VerifyScriptBatch(batch)
			verifyScriptBatchElapsed += time.Since(verifyBatchStart)

			// Process results
			for _, err := range results {
				if err != nil {
					nbFailed++
				}
			}

			// Clear batch for reuse
			batch.Clear()
			batchIndices = batchIndices[:0]
		}
	}

	if nbFailed > 0 {
		log.Printf("Batch verification: %d failed out of %d", nbFailed, len(csvData))
	}

	return verifyScriptBatchElapsed, batchAddElapsed
}
