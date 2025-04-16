package main

import (
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/gocarina/gocsv"
)

const CSVHeaders = "ChainNet,BlockHeight,TXID,TxHexExtended, UTXOHeights"

// CsvDataRecord hold a data record for csv file
type CsvDataRecord struct {
	ChainNet        string
	BlockHeight     int32
	TXID            string
	TxHexExtended   string
	UTXOHeights     string // string joinning utxo heights with separator |
	DataUTXOHeights []int32
	TxBinExtended   []byte
}

// CSVLine print the data record to a string line
func (d *CsvDataRecord) CSVLine() string {

	// Fill in the string of utxo heights if it is empty and the list
	// of utxo heights is not empty
	if len(d.UTXOHeights) < 1 {
		if len(d.DataUTXOHeights) > 0 {
			hSlice := make([]string, len(d.DataUTXOHeights))
			for i, h := range d.DataUTXOHeights {
				hSlice[i] = strconv.FormatUint(uint64(h), 10)
			}
			d.UTXOHeights = strings.Join(hSlice, "|")
		}
	}

	return fmt.Sprintf("%v,%v,%v,%v,%v", d.ChainNet, d.BlockHeight, d.TXID, d.TxHexExtended, d.UTXOHeights)
}

// Read CSV File and return the slice of records
func ReadCSVFile(filepath string) ([]CsvDataRecord, error) {
	ret := []CsvDataRecord{}

	file, err := os.OpenFile(filepath, os.O_RDONLY, os.ModePerm)
	if err != nil {
		return ret, fmt.Errorf("error opening file : %v. error : %v", filepath, err)
	}
	defer file.Close()

	if err := gocsv.UnmarshalFile(file, &ret); err != nil {
		return ret, fmt.Errorf("error parsing file : %v. error : %v", filepath, err)
	}

	// Post process, trim all leading and trailing whitespace
	for i := 0; i < len(ret); i++ {
		ret[i].ChainNet = strings.TrimSpace(ret[i].ChainNet)
		ret[i].TXID = strings.TrimSpace(ret[i].TXID)
		ret[i].TxHexExtended = strings.TrimSpace(ret[i].TxHexExtended)
		ret[i].UTXOHeights = strings.TrimSpace(ret[i].UTXOHeights)

		// Preparse binary tx
		txBin, err := hex.DecodeString(ret[i].TxHexExtended)
		if err != nil {
			return ret, fmt.Errorf("failed to decode hex for line %v, TxID %v, error %v", i, ret[i].TXID, err)
		}
		ret[i].TxBinExtended = txBin

		// Parse utxo heights
		if len(ret[i].UTXOHeights) > 0 {
			parts := strings.Split(ret[i].UTXOHeights, "|")
			ret[i].DataUTXOHeights = make([]int32, len(parts))

			for k, p := range parts {
				h, err := strconv.ParseUint(p, 10, 32)
				if err != nil {
					return ret, fmt.Errorf("error parsing utxo height at line %v, error :%v", i, err)
				}
				ret[i].DataUTXOHeights[k] = int32(h)
			}
		}
	}

	return ret, nil
}
