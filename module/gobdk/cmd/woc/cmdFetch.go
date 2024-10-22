package main

import (
	"encoding/csv"
	"errors"
	"fmt"
	"log"
	"log/slog"
	"os"
	"strconv"
	"strings"
	"time"

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
var skipMandatoryBlock = false

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
	cmdFetch.Flags().BoolVarP(&skipMandatoryBlock, "skip-mandatory", "s", false, fmt.Sprintf("Skip fetching mandatory blocks. It means all blocks will be fully fetched. This options is useful when fetching a few full blocks. Default %v", false))

	cmdRoot.AddCommand(cmdFetch)
}

func execFetch(cmd *cobra.Command, args []string) {
	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	data := NewCSVDataWriter(api, cmdFetchFilePath)
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

	// If data file already exist, then fetch from its last block height
	if data.lastBlock > 0 {
		minBlock = max(data.lastBlock, minBlock)
	}

	mandatoryBlocks := []uint64{}
	if !skipMandatoryBlock {
		mandatoryBlocks = woc.GetMandatoryBlocks(network)
	} else {
		slog.Warn("Used of --skip-mandatory, all blocks will be fetched fully")
	}

	blockStep := ((maxBlock - minBlock) / maxNbTx) + 1 // Always positive
	iBlock := minBlock
	startTime := time.Now()
	for iBlock < maxBlock {
		// Fetch/drain the mandatory blocks in higher priority
		for len(mandatoryBlocks) > 0 && iBlock > mandatoryBlocks[0] {
			mBlock := mandatoryBlocks[0]
			slog.Info(fmt.Sprintf("Fetching full mandatory block %v", mBlock))
			if err := data.fetchBlock(mBlock, 0); err != nil {
				errStr := fmt.Sprintf("Failed to fetch mandatory block %v, error : %v", mBlock, err)
				var notFoundErr *emptyBlockError
				if errors.As(err, &notFoundErr) {
					slog.Warn(errStr)
				} else {
					slog.Error(errStr)
				}

			}
			mandatoryBlocks = mandatoryBlocks[1:]
		}

		// Fetch 1 tx for the "normal" block. If skipMandatoryBlock is used
		// All blocks will be fully fetched. This option is useful when download a few single blocks
		nbTxInBlock := 1
		if skipMandatoryBlock {
			nbTxInBlock = 0
		}
		if err := data.fetchBlock(iBlock, nbTxInBlock); err != nil {
			var notFoundErr *emptyBlockError
			if errors.As(err, &notFoundErr) {
				slog.Warn(fmt.Sprintf("Modifying the next block to fetch due to the empty block %v, next fetch will be %v", iBlock, iBlock+1))
				iBlock += 1
				continue
			} else {
				slog.Error(fmt.Sprintf("Failed to fetch for block %v, error \n%v\n", iBlock, err))
			}
		}

		iBlock += blockStep
	}

	elapsed := time.Since(startTime)
	tps := float64(data.txCount) / elapsed.Seconds()
	log.Printf("TOTAL fetched %v txs, network %v within %v, Average TPS %.2f", data.txCount, network, elapsed, tps)
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

// emptyBlockError is thrown in case a block does not have any tx,
// or equivalently the block have only a coinbase transaction
type emptyBlockError struct {
	blockHeight uint64
}

func (e *emptyBlockError) Error() string {
	return fmt.Sprintf("empty block %v", e.blockHeight)
}

func NewEmptyBlockErrorError(h uint64) *emptyBlockError {
	return &emptyBlockError{
		blockHeight: h,
	}
}

// csvDataWriter hold the data to handle smoothly the I/O operations
type csvDataWriter struct {
	api       *woc.APIClient
	file      *os.File
	filepath  string
	txCount   uint64
	lastBlock uint64
}

func NewCSVDataWriter(api *woc.APIClient, f string) *csvDataWriter {

	fileExisted := fileExists(f)
	file, err := os.OpenFile(f, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		panic(fmt.Sprintf("Error opening file: %v", err))
	}

	iLine, iBlock := uint64(0), uint64(0)
	if !fileExisted {
		file.WriteString(fmt.Sprintf("%v\n", CSVHeaders))
	} else {
		// Read the existing csv file
		file, err := os.Open(f)
		if err != nil {
			log.Fatalf("Failed to open file: %s", err)
		}
		defer file.Close()

		reader := csv.NewReader(file)

		for {
			record, err := reader.Read() // Read one line (record)
			if err != nil {
				if err.Error() == "EOF" { // If end of file is reached, break
					break
				}
				log.Fatalf("Error reading file %v:%v, error %s", f, iLine, err)
			}

			// Start processing only from second line
			if iLine > 0 {
				blockHeightStr := strings.TrimSpace(record[1])
				blockHeight, err := strconv.ParseUint(blockHeightStr, 10, 64)
				if err != nil {
					log.Fatalf("Error parsing block height  %v:%v, error %s", f, iLine, err)
				}
				if blockHeight > iBlock {
					iBlock = blockHeight
				}
			}
			iLine += 1
		}

	}

	return &csvDataWriter{
		api:       api,
		file:      file,
		filepath:  f,
		txCount:   iLine - 1,
		lastBlock: iBlock,
	}
}

// fetchBlock fetch the full extended txs from the provided block height
// if nbTx < 1, then fetch the full block
func (d *csvDataWriter) fetchBlock(blockHeight uint64, nbTx int) error {
	listTxID, errListTxID := woc.GetListTxFromBlock(d.api, blockHeight)
	if errListTxID != nil {
		return fmt.Errorf("network : %v, block %v, failed to get list of txIDs\n\nerror\n%w", network, blockHeight, errListTxID)
	}

	if len(listTxID) < 1 {
		return NewEmptyBlockErrorError(blockHeight)
	}

	nbTxToFetch := min(len(listTxID), nbTx)
	if nbTx == 0 {
		nbTxToFetch = len(listTxID)
	}

	txToFetch := listTxID[:nbTxToFetch]
	slog.Info(fmt.Sprintf("Fetching %v txs for block %v", nbTxToFetch, blockHeight))

	aggregatedErrStr := ""
	for _, txID := range txToFetch {
		txHexExtended, errTxHexExtended := woc.GetTxHexExtended(d.api, txID)
		if errTxHexExtended != nil {
			aggregatedErrStr += fmt.Sprintf("Failed to fetch Tx : %v, Block : %v, Network %v. Error \n%v\n\n", txID, blockHeight, network, errTxHexExtended)
		}

		record := CsvDataRecord{
			ChainNet:      network,
			BlockHeight:   blockHeight,
			TXID:          txID,
			TxHexExtended: txHexExtended,
		}

		csvLine := fmt.Sprintf("%v\n", record.CSVLine())
		d.file.WriteString(csvLine)
		//fmt.Println(csvLine)
		d.txCount += 1
	}

	if len(aggregatedErrStr) > 1 {
		return fmt.Errorf("error fetching %v txs for block %v, network %v, \n%v", len(txToFetch), blockHeight, network, aggregatedErrStr)
	}

	return nil
}

func (d *csvDataWriter) Close() {
	d.file.Close()
}
