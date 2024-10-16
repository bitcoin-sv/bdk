package main

import (
	"fmt"

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
	cmdTX.MarkFlagRequired("txID") // Make the --txID flag required

	cmdTX.Flags().BoolVarP(&cmdTxuseStandard, "standard", "s", false, "Fetch only the standard transaction hex")
	cmdRoot.AddCommand(cmdTX)
}

func execTX(cmd *cobra.Command, args []string) {

	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	if cmdTxuseStandard {
		if txHex, err := woc.GetTxHex(api, cmdTXtxID); err != nil {
			panic(err)
		} else {
			fmt.Println(txHex)
		}
	} else {
		if txHex, err := woc.GetTxHexExtended(api, cmdTXtxID); err != nil {
			panic(err)
		} else {
			fmt.Printf("\nNetwork : %v, txID : %v,  std : %v , Hex\n\n%v\n", network, cmdTXtxID, cmdTxuseStandard, txHex)
		}
	}
}
