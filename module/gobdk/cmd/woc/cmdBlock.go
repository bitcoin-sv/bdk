package main

import (
	"fmt"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var cmdBlockHeight int32
var cmdBlockHeaderOnly bool

// cmdBlock represents the block command
var cmdBlock = &cobra.Command{
	Use:   "block",
	Short: "Retrieve block information",
	Long: `
Use this command to retrieve information for a specific block based on height.
Example :
    go run ./cmd/woc/ block --network main --block-height 10000
	go run ./cmd/woc/ block --network main --block-height 10000 --header-only
`,
	Run: execBlock,
}

func init() {
	// Define the --height flag for the block command
	cmdBlock.Flags().Int32VarP(&cmdBlockHeight, "block-height", "b", 0, "Block height (required)")
	cmdBlock.Flags().BoolVarP(&cmdBlockHeaderOnly, "header-only", "o", false, fmt.Sprintf("Fetch the block header only"))

	// Make the --block-height flag required
	if err := cmdBlock.MarkFlagRequired("block-height"); err != nil {
		panic(err)
	}

	cmdRoot.AddCommand(cmdBlock)
}

func execBlock(cmd *cobra.Command, args []string) {

	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	var jsonStr string
	var err error

	if cmdBlockHeaderOnly {
		jsonStr, err = woc.GetBlockHeader(api, fmt.Sprintf("%v", cmdBlockHeight))
	} else {
		jsonStr, err = woc.GetBlockByHeight(api, cmdBlockHeight)
	}

	if err != nil {
		panic(err)
	} else {
		fmt.Printf("\nNetwork : %v,  Block Height : %v\n\n%v\n", network, cmdBlockHeight, jsonStr)
	}
}
