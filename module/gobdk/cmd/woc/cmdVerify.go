package main

import (
	"fmt"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var filePath string

// cmdVerify represents the test command
var cmdVerify = &cobra.Command{
	Use:   "verify",
	Short: "Run a test based on a file",
	Long:  `Use this command to run tests from a specific file.`,
	Run:   execVerify,
}

func init() {
	// Define the --file-path flag for the test command
	cmdVerify.Flags().StringVarP(&filePath, "file-path", "f", "", "Path to the test file (required)")
	cmdVerify.MarkFlagRequired("file-path") // Make the --file-path flag required

	cmdRoot.AddCommand(cmdVerify)
}

func execVerify(cmd *cobra.Command, args []string) {
	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)
	//txID : 2d05f0c9c3e1c226e63b5fac240137687544cf631cd616fd34fd188fc9020866
	//if json, err := woc.GetBlockChainInfo(api); err != nil {
	//if json, err := woc.GetBlockByHeight(api, 100); err != nil {
	//if json, err := woc.GetTxHex(api, "2d05f0c9c3e1c226e63b5fac240137687544cf631cd616fd34fd188fc9020866"); err != nil {
	//if json, err := woc.GetChainTip(api); err != nil {
	//if json, err := woc.GetTxHexExtended(api, "654cf5a35962eb2f2404666187b0f9759e802b0c54e1a7ed05efd09e7d041423"); err != nil {
	if json, err := woc.GetListTxFromBlock(api, 107000); err != nil {
		panic(err)
	} else {
		fmt.Println(json)
	}

	fmt.Printf("Fetching block at height: %d on network: %s\n", cmdBlockHeight, network)
}
