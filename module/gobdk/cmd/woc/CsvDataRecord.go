package main

import (
	"fmt"
	"os"
	"strings"

	"github.com/gocarina/gocsv"
)

const CSVHeaders = "ChainNet,BlockHeight,TXID,TxHexExtended"

// CsvDataRecord hold a data record for csv file
type CsvDataRecord struct {
	ChainNet      string
	BlockHeight   uint64
	TXID          string
	TxHexExtended string
}

// CSVLine print the data record to a string line
func (d *CsvDataRecord) CSVLine() string {
	return fmt.Sprintf("%v,%v,%v,%v", d.ChainNet, d.BlockHeight, d.TXID, d.TxHexExtended)
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
	}

	return ret, nil
}
