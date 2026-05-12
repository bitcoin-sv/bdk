package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var cmdTXtxID string
var cmdTxuseStandard = false
var cmdTxCsvFile = ""

// cmdTX represents the tx command
var cmdTX = &cobra.Command{
	Use:   "tx",
	Short: "Retrieve transaction hex (enriched with utxo information)",
	Long: `
Use this command to fetch an extended transaction given its txID. If --standard, then just fetch the transaction hex.
If --csv-file is provided, write a single-line CSV record (same schema as 'fetch') to that path instead of printing.
Examples :
	go run ./cmd/woc/ tx --network main --txID 654cf5a35962eb2f2404666187b0f9759e802b0c54e1a7ed05efd09e7d041423
	go run ./cmd/woc/ tx --network main --txID 654cf5a35962eb2f2404666187b0f9759e802b0c54e1a7ed05efd09e7d041423 --csv-file ./data/tx.csv
`,
	PersistentPreRunE: checkCmdTXArguments,
	Run:               execTX,
}

func init() {
	// Define the --txID flag for the tx command
	cmdTX.Flags().StringVarP(&cmdTXtxID, "txID", "i", "", "Transaction ID (required)")

	// Make the --txID flag required
	if err := cmdTX.MarkFlagRequired("txID"); err != nil {
		panic(err)
	}

	cmdTX.Flags().BoolVarP(&cmdTxuseStandard, "standard", "s", false, "Fetch only the standard transaction hex")
	cmdTX.Flags().StringVarP(&cmdTxCsvFile, "csv-file", "o", "", "Optional output CSV file. Parent directory must exist and the file must not exist yet. Incompatible with --standard.")

	cmdRoot.AddCommand(cmdTX)
}

// checkCmdTXArguments validates the cmdTX flag combination and, when
// --csv-file is set, that the parent directory exists and the file does not.
func checkCmdTXArguments(cmd *cobra.Command, args []string) error {
	if cmdTxCsvFile == "" {
		return nil
	}

	if cmdTxuseStandard {
		return fmt.Errorf("--csv-file is incompatible with --standard (CSV records carry the extended tx hex and utxo heights)")
	}

	dir := filepath.Dir(cmdTxCsvFile)
	dirInfo, err := os.Stat(dir)
	if err != nil {
		if os.IsNotExist(err) {
			return fmt.Errorf("parent directory %q does not exist", dir)
		}
		return fmt.Errorf("cannot stat parent directory %q: %w", dir, err)
	}
	if !dirInfo.IsDir() {
		return fmt.Errorf("parent path %q is not a directory", dir)
	}

	if _, err := os.Stat(cmdTxCsvFile); err == nil {
		return fmt.Errorf("conflict: file %q already exists, refusing to overwrite", cmdTxCsvFile)
	} else if !os.IsNotExist(err) {
		return fmt.Errorf("cannot stat output path %q: %w", cmdTxCsvFile, err)
	}

	return nil
}

func execTX(cmd *cobra.Command, args []string) {

	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	if cmdTxCsvFile != "" {
		execTXToCSV(api)
		return
	}

	if cmdTxuseStandard {
		if tx, err := woc.GetBulkTx(api, []string{cmdTXtxID}); err != nil {
			panic(err)
		} else {
			if len(tx) != 1 {
				panic(fmt.Errorf("error returned list of transaction len=%v while expect to be 1", len(tx)))
			}
			fmt.Println(tx[0].Hex)
		}
	} else {
		if txHex, utxoHeights, _, err := woc.GetTxHexExtended(api, cmdTXtxID); err != nil {
			panic(err)
		} else {
			hSlice := make([]string, len(utxoHeights))
			for i, h := range utxoHeights {
				hSlice[i] = strconv.FormatUint(uint64(h), 10) // Convert uint64 to string
			}
			utxoHeightsStr := strings.Join(hSlice, "|")

			fmt.Printf("\nNetwork : %v, txID : %v,  std : %v\n\nTxHex\n%v\n\nUTXO Heights\n\"%v\"\n\n", network, cmdTXtxID, cmdTxuseStandard, txHex, utxoHeightsStr)
		}
	}
}

// execTXToCSV fetches the extended tx and writes a single CSV record using
// the same schema as cmdFetch (header line + one data line). The output file
// is opened with O_EXCL so a concurrent creation between the pre-run check
// and the open is reported as an error rather than silently overwritten.
func execTXToCSV(api *woc.APIClient) {
	txHex, utxoHeights, blockHeight, err := woc.GetTxHexExtended(api, cmdTXtxID)
	if err != nil {
		panic(err)
	}

	file, err := os.OpenFile(cmdTxCsvFile, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0644)
	if err != nil {
		panic(fmt.Errorf("failed to create CSV file %q: %w", cmdTxCsvFile, err))
	}
	defer file.Close()

	if _, err := file.WriteString(CSVHeaders + "\n"); err != nil {
		panic(fmt.Errorf("failed to write CSV header to %q: %w", cmdTxCsvFile, err))
	}

	record := CsvDataRecord{
		ChainNet:        network,
		BlockHeight:     blockHeight,
		TXID:            cmdTXtxID,
		TxHexExtended:   txHex,
		DataUTXOHeights: utxoHeights,
	}
	if _, err := file.WriteString(record.CSVLine() + "\n"); err != nil {
		panic(fmt.Errorf("failed to write CSV record to %q: %w", cmdTxCsvFile, err))
	}

	fmt.Printf("Wrote 1 record to %v (network=%v, blockHeight=%v, txID=%v)\n",
		cmdTxCsvFile, network, blockHeight, cmdTXtxID)
}
