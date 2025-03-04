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
var importantBlocks = map[string][]uint32{
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
func GetMandatoryBlocks(network string) []uint32 {
	if blocks, ok := importantBlocks[network]; ok {

		// Sort before returning the blocks slice
		sort.Slice(blocks, func(i, j int) bool {
			return blocks[i] < blocks[j]
		})
		return blocks
	}
	return []uint32{}
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
func GetBlockByHeight(api *APIClient, h uint32) (string, error) {
	path := fmt.Sprintf("block/height/%v", h)
	return api.Fetch(path)
}

// GetTxDetail given the txID, return the tx detail in json format
// See : https://docs.taal.com/core-products/whatsonchain/transaction#get-raw-transaction-data
func GetTxDetail(api *APIClient, txID string) (string, error) {
	path := fmt.Sprintf("tx/%v", txID)
	return api.Fetch(path)
}

type TxData struct {
	TxID          string `json:"txid"`
	Hex           string `json:"hex"`
	BlockHash     string `json:"blockhash"`
	BlockHeight   uint32 `json:"blockheight"`
	BlockTime     int64  `json:"blocktime"`
	Confirmations int    `json:"confirmations"`
}

type BulkRequest struct {
	TxIDs []string `json:"txids"`
}

// GetTxHex given the txID, return the tx hex
// See : https://docs.taal.com/core-products/whatsonchain/transaction#bulk-raw-transaction-data
func GetBulkTx(api *APIClient, TxIDs []string) ([]TxData, error) {
	req := BulkRequest{TxIDs: TxIDs}
	jsonData := ""

	if binData, err := json.Marshal(req); err != nil {
		return nil, fmt.Errorf("failed to encode transaction list, error %w", err)
	} else {
		jsonData = string(binData)
	}

	var txs []TxData
	if respStr, err := api.Post("txs/hex", jsonData); err != nil {
		return nil, err
	} else {
		if err := json.Unmarshal([]byte(respStr), &txs); err != nil {
			return nil, fmt.Errorf("failed to decode transaction list, error %w", err)
		}
	}
	return txs, nil
}

// GetTxHexExtended given the txID, return the extended tx hex
// And the list of utxo heights
// See  tx ExtendedBytes()
//
//	https://github.com/ordishs/go-bt/blob/master/tx.go#L342
func GetTxHexExtended(api *APIClient, txID string) (string, []uint32, error) {

	// Get the standard tx from woc
	txsMain, err := GetBulkTx(api, []string{txID})
	if err != nil {
		return "", []uint32{}, fmt.Errorf("failed to get tx hex from with id %v, error %w", txID, err)
	}

	if len(txsMain) != 1 {
		return "", []uint32{}, fmt.Errorf("error returned list of transaction len=%v while expect to be 1", len(txsMain))
	}

	txHex := txsMain[0].Hex

	tx, err := bt.NewTxFromString(txHex)
	if err != nil {
		return "", []uint32{}, fmt.Errorf("failed to parse tx hex %v, error %w", txHex, err)
	}

	// enrich the extended tx by fetching the UTXOs
	parentTxs := make(map[string]*bt.Tx)
	parentTxsHeight := make(map[string]uint32)
	utxoHeights := make([]uint32, len(tx.Inputs))
	for i, input := range tx.Inputs {
		parentTxID := input.PreviousTxIDChainHash().String()
		parentTx, ok := parentTxs[parentTxID]
		if !ok {
			// get the parent tx and store in the map
			parentTxID := input.PreviousTxIDChainHash().String()

			// Get the standard tx from woc
			txs, err := GetBulkTx(api, []string{parentTxID})
			if err != nil {
				return "", []uint32{}, fmt.Errorf("failed to get parent tx with id %v, error %w", parentTxID, err)
			}

			if len(txs) != 1 {
				return "", []uint32{}, fmt.Errorf("error returned list of transaction len=%v while expect to be 1", len(txs))
			}

			parentTxHex := txs[0].Hex

			parentTx, err = bt.NewTxFromString(parentTxHex)
			if err != nil {
				return "", []uint32{}, fmt.Errorf("failed to parse parent tx for input %v, hex %v , error %w", i, parentTxHex, err)
			}

			parentTxs[parentTxID] = parentTx
			parentTxsHeight[parentTxID] = txs[0].BlockHeight
		}

		// add the parent tx output to the input
		previousScript, err := hex.DecodeString(parentTx.Outputs[input.PreviousTxOutIndex].LockingScript.String())
		if err != nil {
			return "", []uint32{}, fmt.Errorf("failed to the utxo for the input %v, error %w", i, err)
		}

		utxoHeights[i] = parentTxsHeight[parentTxID]
		tx.Inputs[i].PreviousTxScript = bscript.NewFromBytes(previousScript)
		tx.Inputs[i].PreviousTxSatoshis = parentTx.Outputs[input.PreviousTxOutIndex].Satoshis
	}

	return hex.EncodeToString(tx.ExtendedBytes()), utxoHeights, nil
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

type partChainInfoBlockHeight struct {
	Blocks uint32 `json:"blocks"`
}

// GetChainTip return the highest block
// See : https://docs.taal.com/core-products/whatsonchain/chain-info#get-blockchain-info

func GetChainTip(api *APIClient) (uint32, error) {
	if jsonStr, err := GetBlockChainInfo(api); err != nil {
		return uint32(0), err
	} else {
		var data partChainInfoBlockHeight
		if err := json.Unmarshal([]byte(jsonStr), &data); err != nil {
			return uint32(0), fmt.Errorf("unable to parse the json body, error %w", err)
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
func GetListTxFromBlock(api *APIClient, blockHeight uint32) ([]string, error) {
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
