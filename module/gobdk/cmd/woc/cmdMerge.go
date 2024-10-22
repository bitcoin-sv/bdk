package main

import (
	"fmt"
	"log"
	"os"
	"sort"

	"github.com/spf13/cobra"
)

var cmdMergeFileA string
var cmdMergeFileB string
var cmdMergeFileOut string

// cmdMerge merge 2 csv files
var cmdMerge = &cobra.Command{
	Use:     "merge",
	Short:   "Merge data from 2 csv files",
	Long:    "Merge data from 2 csv files that ensure the blocks ordering",
	Example: "go run ./cmd/woc/ merge --file-a ./data/small_test1.csv --file-b ./data/small_test2.csv --file-out ./data/small_test_merged.csv",
	Run:     execMerge,
}

func init() {
	cmdMerge.Flags().StringVarP(&cmdMergeFileA, "file-a", "a", "", "Path to the first file (required)")
	cmdMerge.MarkFlagRequired("file-a")

	cmdMerge.Flags().StringVarP(&cmdMergeFileB, "file-b", "b", "", "Path to the second file (required)")
	cmdMerge.MarkFlagRequired("file-b")

	cmdMerge.Flags().StringVarP(&cmdMergeFileOut, "file-out", "o", "", "Path to the ouput (merged file)")
	cmdMerge.MarkFlagRequired("file-out")

	cmdRoot.AddCommand(cmdMerge)
}

func execMerge(cmd *cobra.Command, args []string) {
	fileOut, err := os.OpenFile(cmdMergeFileOut, os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		panic(fmt.Sprintf("Error creating file: %v", err))
	}
	defer fileOut.Close()
	fileOut.WriteString(fmt.Sprintf("%v\n", CSVHeaders))

	dataA, errA := ReadCSVFile(cmdMergeFileA)
	if errA != nil {
		log.Fatal(errA)
	}

	dataB, errB := ReadCSVFile(cmdMergeFileB)
	if errB != nil {
		log.Fatal(errB)
	}

	dataOut := append(dataA, dataB...)
	sort.Slice(dataOut, func(i, j int) bool {
		return dataOut[i].BlockHeight < dataOut[j].BlockHeight
	})

	for _, record := range dataOut {
		csvLine := fmt.Sprintf("%v\n", record.CSVLine())
		//fmt.Println(csvLine)
		fileOut.WriteString(csvLine)
	}
}
