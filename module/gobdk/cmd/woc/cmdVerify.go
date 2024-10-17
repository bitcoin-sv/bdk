package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"os"

	"github.com/gocarina/gocsv"
	"github.com/libsv/go-bt/v2"
	"github.com/spf13/cobra"
)

var cmdVerifyFilePath string

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
	cmdVerify.MarkFlagRequired("file-path")

	cmdRoot.AddCommand(cmdVerify)
}

type csvDataRecord struct {
	ChainNet      string
	BlockHeight   uint64
	TXID          string
	TxHexExtended string
}

func execVerify(cmd *cobra.Command, args []string) {

	file, err := os.OpenFile(cmdVerifyFilePath, os.O_RDWR|os.O_CREATE, os.ModePerm)
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	var csvData []csvDataRecord
	if err := gocsv.UnmarshalFile(file, &csvData); err != nil {
		log.Fatal(err)
	}

	for i, record := range csvData {
		fmt.Printf("Person %d: %+v\n", i, record)

		tx, err := bt.NewTxFromString(record.TxHexExtended)
		if err != nil || tx == nil {
			log.Println(fmt.Errorf("error parsing extended tx at record %v, txID : %v", i, record.TXID))
			continue
		}

		txHexExtended := hex.EncodeToString(tx.ExtendedBytes())
		if txHexExtended != record.TxHexExtended {
			log.Fatal(fmt.Errorf("error recovering extended tx at record %v, txID : %v", i, record.TXID))
		} else {
			log.Println("tx ok")
		}
	}
}
