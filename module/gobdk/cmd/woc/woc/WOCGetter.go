package woc

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"sort"

	"github.com/libsv/go-bt/v2"
	"github.com/libsv/go-bt/v2/bscript"
)

// Define and initialize the map (as a variable, not constant)
var importantBlocks = map[string][]uint64{
	"main": {
		227930,
		227931, //main.BIP34Height
		227932,
		388380,
		388381, //main.BIP65Height
		388382,
		363724,
		363725, //main.BIP66Height
		363726,
		419327,
		419328, //main.CSVHeight
		419329,
		478557,
		478558, //main.uahfHeight
		478559,
		504030,
		504031, //main.daaHeight
		504032,
		620537,
		620538, //main.genesisHeight
		620539,
		//882686,
		//882687, //main.chronicleHeight
		//882688,
	},
	"test": {
		21110,
		21111, //test.BIP34Height
		21112,
		581884,
		581885, //test.BIP65Height
		581886,
		330775,
		330776, //test.BIP66Height
		330777,
		770111,
		770112, //test.CSVHeight
		770113,
		1155874,
		1155875, //test.uahfHeight
		1155876,
		1188696,
		1188697, //test.daaHeight
		1188698,
		1344301,
		1344302, //test.genesisHeight
		1344303,
		//1621669,
		//1621670, //test.chronicleHeight
		//1621671,
	},
	"stn": {
		99999999,
		100000000, //stn.BIP34Height
		100000001,
		14,
		15, //stn.uahfHeight
		16,
		2199,
		2200, //stn.daaHeight
		2201,
		99,
		100, //stn.genesisHeight
		101,
		//249,
		//250,       //stn.chronicleHeight
		//251,
	},
}

// GetMandatoryBlocks returns all the important blocks as marked in node params
func GetMandatoryBlocks(network string) []uint64 {
	if blocks, ok := importantBlocks[network]; ok {

		// Sort before returning the blocks slice
		sort.Slice(blocks, func(i, j int) bool {
			return blocks[i] < blocks[j]
		})
		return blocks
	}
	return []uint64{}
}

/////////////////////////////////////////////////////////////////////////////////////////////////

// GetChainInfo return the JSON body of the chaininfo
// See : https://docs.taal.com/core-products/whatsonchain/chain-info#get-blockchain-info
func GetBlockChainInfo(api *APIClient) (string, error) {
	path := "chain/info"
	return api.Fetch(path)
}

// GetBlockByHeight given the block height, return the JSON body of the block detail
// See : https://docs.taal.com/core-products/whatsonchain/block#get-by-height
func GetBlockByHeight(api *APIClient, h uint64) (string, error) {
	path := fmt.Sprintf("block/height/%v", h)
	return api.Fetch(path)
}

// GetTxDetail given the txID, return the tx detail in json format
// See : https://docs.taal.com/core-products/whatsonchain/transaction#get-raw-transaction-data
func GetTxDetail(api *APIClient, txID string) (string, error) {
	path := fmt.Sprintf("tx/%v", txID)
	return api.Fetch(path)
}

// GetTxHex given the txID, return the tx hex
// See : https://docs.taal.com/core-products/whatsonchain/transaction#get-raw-transaction-data
func GetTxHex(api *APIClient, txID string) (string, error) {
	path := fmt.Sprintf("tx/%v/hex", txID)
	return api.Fetch(path)
}

// GetTxHexExtended given the txID, return the extended tx hex
// See  tx ExtendedBytes()
//
//	https://github.com/ordishs/go-bt/blob/master/tx.go#L342
func GetTxHexExtended(api *APIClient, txID string) (string, error) {

	// Get the standard tx from woc
	txHex, err := GetTxHex(api, txID)
	if err != nil {
		return "", fmt.Errorf("failed to get tx hex from with id %v, error %w", txID, err)
	}

	tx, err := bt.NewTxFromString(txHex)
	if err != nil {
		return "", fmt.Errorf("failed to parse tx hex %v, error %w", txHex, err)
	}

	// enrich the extended tx by fetching the UTXOs
	parentTxs := make(map[string]*bt.Tx)
	for i, input := range tx.Inputs {
		parentTx, ok := parentTxs[input.PreviousTxIDChainHash().String()]
		if !ok {
			// get the parent tx and store in the map
			parentTxID := input.PreviousTxIDChainHash().String()

			parentTxHex, err := GetTxHex(api, parentTxID)
			if err != nil {
				return "", fmt.Errorf("failed to fetch the parent tx, input : %v, parentTxID %v, , error %w", i, parentTxID, err)
			}

			parentTx, err = bt.NewTxFromString(parentTxHex)
			if err != nil {
				return "", fmt.Errorf("failed to parse parent tx for input %v, hex %v , error %w", i, parentTxHex, err)
			}

			parentTxs[parentTxID] = parentTx
		}

		// add the parent tx output to the input
		previousScript, err := hex.DecodeString(parentTx.Outputs[input.PreviousTxOutIndex].LockingScript.String())
		if err != nil {
			return "", fmt.Errorf("failed to the utxo for the input %v, error %w", i, err)
		}

		tx.Inputs[i].PreviousTxScript = bscript.NewFromBytes(previousScript)
		tx.Inputs[i].PreviousTxSatoshis = parentTx.Outputs[input.PreviousTxOutIndex].Satoshis
	}

	return hex.EncodeToString(tx.ExtendedBytes()), nil
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

type partChainInfoBlockHeight struct {
	Blocks uint64 `json:"blocks"`
}

// GetChainTip return the highest block
// See : https://docs.taal.com/core-products/whatsonchain/chain-info#get-blockchain-info

func GetChainTip(api *APIClient) (uint64, error) {
	if jsonStr, err := GetBlockChainInfo(api); err != nil {
		return uint64(0), err
	} else {
		var data partChainInfoBlockHeight
		if err := json.Unmarshal([]byte(jsonStr), &data); err != nil {
			return uint64(0), fmt.Errorf("unable to parse the json body, error %w", err)
		} else {
			return data.Blocks, nil
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

type partCoinbseTx struct {
	TxID string `json:"txid"`
}
type partBlockTXs struct {
	Tx         []string      `json:"tx"`
	CoinbaseTX partCoinbseTx `json:"coinbaseTx"`
}

// GetChainTip return the highest block
// The list of returned txIDs has removed the coinbase tx
// See : https://docs.taal.com/core-products/whatsonchain/block#get-by-height
func GetListTxFromBlock(api *APIClient, blockHeight uint64) ([]string, error) {
	if jsonStr, err := GetBlockByHeight(api, blockHeight); err != nil {
		return []string{}, err
	} else {
		var data partBlockTXs
		if err := json.Unmarshal([]byte(jsonStr), &data); err != nil {
			return []string{}, fmt.Errorf("unable to parse the json body, error %w", err)
		} else {
			// Remove the coinbase tx from the list tx
			for i, v := range data.Tx {
				if v == data.CoinbaseTX.TxID {
					// Word found, remove it by slicing and concatenating
					return append(data.Tx[:i], data.Tx[i+1:]...), nil
				}
			}
			return data.Tx, nil
		}
	}
}
