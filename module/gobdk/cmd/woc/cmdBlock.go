package main

import (
	"fmt"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var cmdBlockHeight uint32

// cmdBlock represents the block command
var cmdBlock = &cobra.Command{
	Use:   "block",
	Short: "Retrieve block information",
	Long: `
Use this command to retrieve information for a specific block based on height.
Example :
    go run ./cmd/woc/ block --network main --block-height 10000
`,
	Run: execBlock,
}

func init() {
	// Define the --height flag for the block command
	cmdBlock.Flags().Uint32VarP(&cmdBlockHeight, "block-height", "b", 0, "Block height (required)")
	cmdBlock.MarkFlagRequired("block-height") // Make the --block-height flag required

	cmdRoot.AddCommand(cmdBlock)
}

func execBlock(cmd *cobra.Command, args []string) {

	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	if json, err := woc.GetBlockByHeight(api, cmdBlockHeight); err != nil {
		panic(err)
	} else {
		fmt.Printf("\nNetwork : %v,  Block Height : %v\n\n%v\n", network, cmdBlockHeight, json)
	}
}
