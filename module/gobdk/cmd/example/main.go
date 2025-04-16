package main

import (
	"encoding/csv"
	"encoding/hex"
	"fmt"
	"log/slog"
	"os"
	"strconv"
	"strings"

	"github.com/bitcoin-sv/bdk/module/gobdk/cmd/woc/woc"
	"github.com/libsv/go-bt/v2"
)

func getTxFromBinFile() {
	var tx bt.Tx

	f, err := os.Open("splittingTx.bin")
	defer f.Close()

	_, err = tx.ReadFrom(f)
	if err != nil {
		panic(err)
	}

	txID := tx.TxID()
	txHexExtended := hex.EncodeToString(tx.ExtendedBytes())

	fmt.Printf("TxID : %v\n\nTxHexExtended\n%v\n\n", txID, txHexExtended)
}

func getTxIDFromTXHex() {
	txHex := "010000000000000000ef0225894047a817115970e49cc0b77dc2e1fd5d185632c244c055512dc490e82a5a000000006a47304402205bccbecf7d1658032759c4d182a8170174ad7dc687d58555707198a1e230f84f02203ae6804eef77319876223db1b354828e1beeccaeafb6d667879436eef7aa5f3141210300e3b001c4addf714e8c4d5ac1427fb19349d3d05e416e47fc6186cd2d95eb0effffffff962fb15b000000001976a9145632e2f253ac71e2dda1a8dcec6eac384a74251b88ac88ad2770b0bc82235a6414c68d4eb8764750c48178ffa01f6a93ba972dc1e418000000006b483045022100c348ff56a2f00b608fadb4583b76a9a91079bf25b507ef44da562e8aa90fbdd202204fc40afdb76409527a05cfb67fbc67d496e6b0bd720121a26779123b5dd30212412102381da97b922c1584ca700f97650f1a2c2dcdba8a480ff998541504263f5ce551ffffffff00e1f505000000001976a914f9878c4ef91c883c451bee25ca6daf17da20a29688ac0116b16d61000000001976a914cc401501b36a914bc31f2322612b673cbb98a2d788ac00000000"

	tx, err := bt.NewTxFromString(txHex)
	if err != nil {
		panic(err)
	}

	txID := tx.TxID()

	fmt.Println(txID)
}

func getLargeTxData() {
	txs := map[int32]string{
		886761: "62df03d0c21de4479cdd95a7bc04b75f8c22112edd2c9c13325869739c305354",
	}
	csvHeader := "ChainNet,BlockHeight,TXID,TxHexExtended, UTXOHeights"
	csvFile := "./tx_62df03d0.csv"

	network := "main"
	api := woc.NewAPIClient(
		network,
		woc.DefaultRateLimit(),
	)

	// Open the file with write-only mode, create if not exists, and truncate if it exists
	file, err := os.OpenFile(csvFile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		fmt.Println("Error opening file:", err)
		return
	}
	defer file.Close()

	if _, err := file.WriteString(csvHeader + "\n"); err != nil {
		panic(err)
	}

	for h, txID := range txs {
		slog.Info("Fetching ...", "Block", h, "TxID", txID)
		txHex, utxoHeights, err := woc.GetTxHexExtended(api, txID)
		if err != nil {
			panic(err)
		}

		utxoHeightsStr := make([]string, len(utxoHeights))
		for i, v := range utxoHeights {
			utxoHeightsStr[i] = strconv.FormatUint(uint64(v), 10)
		}

		slog.Info("Done", "Block", h, "TxID", txID)
		utxoStr := strings.Join(utxoHeightsStr, "|")
		line := fmt.Sprintf("%v,%v,%v,%v,%v\n", network, h, txID, txHex, utxoStr)
		if _, err := file.WriteString(line); err != nil {
			panic(err)
		}
	}

	fmt.Println("File written successfully", csvFile)
}

func retrieveTxsAfterGenesis() {
	file1 := "./mainnet_14207txs_b886413_WUH.csv"
	file2 := "./mainnet_14207txs_b886413_WUH_after_genesis.csv"

	inputFile, err := os.Open(file1)
	if err != nil {
		fmt.Printf("Error opening file1: %v\n", err)
		return
	}
	defer inputFile.Close()

	reader := csv.NewReader(inputFile)
	records, err := reader.ReadAll()
	if err != nil {
		fmt.Printf("Error reading CSV file:%v\n", err)
		return
	}

	if len(records) == 0 {
		fmt.Printf("Empty CSV file\n")
		return
	}

	outputFile, err := os.Create(file2)
	if err != nil {
		fmt.Printf("Error creating file2 : %v\n", err)
		return
	}
	defer outputFile.Close()

	writer := csv.NewWriter(outputFile)
	defer writer.Flush()

	// Write the header
	if err := writer.Write(records[0]); err != nil {
		fmt.Printf("Error writing header to file2 %v\n", err)
		return
	}

	// Write the filtered records
	afterGenesis := false
	nbTxAfterGenesis := 0
	for i, record := range records[1:] {
		if i%500 == 0 {
			fmt.Printf("Processed %v lines\n", i)
		}

		if !afterGenesis {
			blockheighStr := record[1]
			h, err := strconv.Atoi(blockheighStr)
			if err != nil {
				fmt.Printf("ERROR line %v, unable to read block height, error %v\n", i, err)
				continue
			}

			if h > 620538 {
				afterGenesis = true
				fmt.Printf("INFO Encouter after genesis at line %v\n", i)
			}
		} else {
			if err := writer.Write(record); err != nil {
				fmt.Printf("ERROR writing %vth record to file2, error %v\n", i, err)
				return
			}

			nbTxAfterGenesis += 1
		}
	}

	fmt.Printf("Completed copy after genesis, total %v txs after genesis\n", nbTxAfterGenesis)
	fmt.Printf("Copied File from %v to %v\n", file1, file2)
}

// main code
func main() {
	getTxFromBinFile()
	// getTxIDFromTXHex()
	// getLargeTxData()
	// retrieveTxsAfterGenesis()
}
