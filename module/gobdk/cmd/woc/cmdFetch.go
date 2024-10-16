package main

import (
	"fmt"
	"log/slog"
	"os"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/spf13/cobra"
)

var defaultCmdFetchFilePath = "./data/test.csv"
var cmdFetchFilePath = defaultCmdFetchFilePath

const defaultMinBlock = uint64(107000) // This is the first block that have more than 1 tx
const maxNbTx = uint64(10000)

var nbTx = maxNbTx
var minBlock = defaultMinBlock
var maxBlock = uint64(0)

// fileExists checks if the file exist
func fileExists(filename string) bool {
	_, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return err == nil
}

// checkCmdFetchArguments check if the input arguments are valid
func checkCmdFetchArguments(cmd *cobra.Command, args []string) error {
	if maxBlock > 0 && minBlock >= maxBlock {
		return fmt.Errorf("min block %v is not lower than set max block %v", minBlock, maxBlock)
	}

	if minBlock < defaultMinBlock {
		return fmt.Errorf("min block %v is lower than than expected %v", minBlock, defaultMinBlock)
	}

	if fileExists(cmdFetchFilePath) {
		slog.Warn(fmt.Sprintf("File already exist %v, the programe will append to it", cmdFetchFilePath))
	}
	return nil
}

// cmdFetch represents the test command
var cmdFetch = &cobra.Command{
	Use:               "fetch",
	Short:             "Run fetch data and write (append) to a data csv file",
	Long:              `Run fetch data and write (append) to a data csv file`,
	Example:           `go run ./cmd/woc/ fetch --network main --file-path ./data/test.csv`,
	PersistentPreRunE: checkCmdFetchArguments,
	Run:               execFetch,
}

func init() {

	cmdFetch.Flags().StringVarP(&cmdFetchFilePath, "file-path", "f", defaultCmdFetchFilePath, fmt.Sprintf("Path to the test file (required), default %v", defaultCmdFetchFilePath))

	cmdFetch.Flags().Uint64VarP(&nbTx, "nb-tx", "n", maxNbTx, fmt.Sprintf("number of tx to fetch, default %v", maxNbTx))
	cmdFetch.Flags().Uint64VarP(&minBlock, "block-min", "l", defaultMinBlock, fmt.Sprintf("min block to fetch, defaulted to %v as it is the first block having more than 1 tx", defaultMinBlock))
	cmdFetch.Flags().Uint64VarP(&maxBlock, "block-max", "u", 0, fmt.Sprintf("max block to fetch. Default %v", maxBlock))

	cmdRoot.AddCommand(cmdFetch)
}

func execFetch(cmd *cobra.Command, args []string) {
	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	data := NewCSVData(api, cmdFetchFilePath)
	defer data.Close()

	// If maxBlock is not set, then query the chaintip to get the max block
	if maxBlock < 1 {
		// Get the chain tip
		mB, errMaxBlock := woc.GetChainTip(api)
		if errMaxBlock != nil {
			panic(errMaxBlock)
		} else {
			maxBlock = mB
		}
	}

	blockStep := (maxBlock - minBlock) / maxNbTx
	mandatoryBlocks := woc.GetMandatoryBlocks(network)
	for iBlock := minBlock; iBlock < maxBlock; iBlock += blockStep {

		// Fetch/drain the mandatory blocks in higher priority
		for len(mandatoryBlocks) > 0 && iBlock > mandatoryBlocks[0] {
			mBlock := mandatoryBlocks[0]
			slog.Info(fmt.Sprintf("fetching full mandatory block %v", mBlock))
			if err := data.fetchBlock(mBlock, 0); err != nil {
				slog.Error(fmt.Sprintf("failed to fetch for mandatory block block %v, error \n%v\n", mBlock, err))
			}
			mandatoryBlocks = mandatoryBlocks[1:]
		}

		// Fetch 1 tx on the block
		if err := data.fetchBlock(iBlock, 1); err != nil {
			slog.Error(fmt.Sprintf("failed to fetch for block %v, error \n%v\n", iBlock, err))
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

type csvData struct {
	api       *woc.APIClient
	file      *os.File
	filepath  string
	txCount   uint64
	lastBlock uint64
}

func NewCSVData(api *woc.APIClient, f string) *csvData {

	fileExisted := fileExists(f)
	file, err := os.OpenFile(f, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		panic(fmt.Sprintf("Error opening file: %v", err))
	}

	// Write header if the file doesn't exist before
	// TODO : if file existed, read the last line to get the last block
	//        it can be usefull to skip block if it is already fetched in the data
	if !fileExisted {
		csvHeaderLine := "ChainNet, BlockHeight, TXID, TxHexExtended\n"
		file.WriteString(csvHeaderLine)
	}

	return &csvData{
		api:       api,
		file:      file,
		filepath:  f,
		txCount:   0,
		lastBlock: 0,
	}
}

// fetchBlock fetch the full extended txs from the provided block height
// if nbTx < 1, then fetch the full block
func (d *csvData) fetchBlock(blockHeight uint64, nbTx int) error {
	listTxID, errListTxID := woc.GetListTxFromBlock(d.api, blockHeight)
	if errListTxID != nil {
		return fmt.Errorf("network : %v, block %v, failed to get list of txIDs", network, blockHeight)
	}

	if len(listTxID) < 1 {
		return fmt.Errorf("network : %v, block %v, skip block having only coinbase transaction", network, blockHeight)
	}

	nbTxToFetch := min(len(listTxID), nbTx)
	txToFetch := listTxID[:nbTxToFetch]

	for _, txID := range txToFetch {
		txHexExtended, errTxHexExtended := woc.GetTxHexExtended(d.api, txID)
		if errTxHexExtended != nil {
			slog.Error(fmt.Sprintf("Network : %v, Tx %v, error getting extended transaction. Error \n\n%v\n", network, txID, errTxHexExtended))
		}

		csvLine := fmt.Sprintf("%v, %v, %v, %v\n", network, blockHeight, txID, txHexExtended)
		d.file.WriteString(csvLine)
		//fmt.Println(csvLine)
		d.txCount += 1
	}

	return nil
}

func (d *csvData) Close() {
	d.file.Close()
}
