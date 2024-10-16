package main

import (
	"fmt"
	"os"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var (
	network string
)

// checkNetworkValid check if the network input from args is valid
func checkNetworkValid(cmd *cobra.Command, args []string) error {
	if network != woc.Mainnet && network != woc.Testnet && network != woc.STNnet {
		return fmt.Errorf("invalid network %v. Network should be either %v, %v, %v", network, woc.Mainnet, woc.Testnet, woc.STNnet)
	}
	return nil
}

// cmdRoot represents the base command when called without any subcommands
var cmdRoot = &cobra.Command{
	Use:               "woc",
	Short:             "A CLI tool for fetching data from whatsonchain and test verify script",
	Long:              `A CLI tool for fetching data from whatsonchain and test verify script`,
	PersistentPreRunE: checkNetworkValid,
}

func init() {
	cmdRoot.PersistentFlags().StringVarP(&network, "network", "c", "", "Specify the network chain (required)")
	cmdRoot.MarkPersistentFlagRequired("network")
}

// main adds all child commands to the root command and sets flags appropriately.
func main() {

	if err := cmdRoot.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}
