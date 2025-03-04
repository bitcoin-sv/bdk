package main

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var cmdTXtxID string
var cmdTxuseStandard = false

// cmdTX represents the tx command
var cmdTX = &cobra.Command{
	Use:   "tx",
	Short: "Retrieve transaction hex (enriched with utxo information)",
	Long: `
Use this command to fetch an extended transaction given its txID. If --standard, then just fetch the transaction hex
Example :
	go run ./cmd/woc/ tx --network main --txID 654cf5a35962eb2f2404666187b0f9759e802b0c54e1a7ed05efd09e7d041423
`,
	Run: execTX,
}

func init() {
	// Define the --txID flag for the tx command
	cmdTX.Flags().StringVarP(&cmdTXtxID, "txID", "i", "", "Transaction ID (required)")

	// Make the --txID flag required
	if err := cmdTX.MarkFlagRequired("txID"); err != nil {
		panic(err)
	}

	cmdTX.Flags().BoolVarP(&cmdTxuseStandard, "standard", "s", false, "Fetch only the standard transaction hex")
	cmdRoot.AddCommand(cmdTX)
}

func execTX(cmd *cobra.Command, args []string) {

	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

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
		if txHex, utxoHeights, err := woc.GetTxHexExtended(api, cmdTXtxID); err != nil {
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
