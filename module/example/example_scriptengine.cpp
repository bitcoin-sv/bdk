/// Use it as an example how to add a example

#include <cassert>
#include <iostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>


#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "extendedTx.hpp"
#include "assembler.h"
#include "utilstrencodings.h"
#include "scriptengine.hpp"

namespace po = boost::program_options;
namespace pt = boost::property_tree;


void doVerifyScript(const bsv::CScriptEngine& se, const std::span<const int32_t> utxoHeights, const int32_t blockHeight, const std::string& txID, const std::string& txHexExtended, const bool consensus) {
    const std::vector<uint8_t> etxBin = ParseHex(txHexExtended);
    const std::span<const uint8_t> etx(etxBin.data(), etxBin.size());

    const char* begin{ reinterpret_cast<const char*>(etx.data()) };
    const char* end{ reinterpret_cast<const char*>(etx.data() + etx.size()) };
    CDataStream is(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    is>> eTX;

    ////Recover the hex
    CDataStream os(SER_NETWORK, PROTOCOL_VERSION);
    os << eTX;
    const std::vector<uint8_t> outBin(os.begin(), os.end());
    const std::string recovTxHexExtended = bsv::Bin2Hex(outBin);
    if (txHexExtended != recovTxHexExtended) {
        throw std::runtime_error("ERROR recover extended hex for TxID " + txID);
    }

    const std::string recovTxID = eTX.mtx.GetId().ToString();
    if (txID != recovTxID) {
        throw std::runtime_error("ERROR recover txID for TxID " + txID);
    }

    if (consensus) {
        const bitcoinconsensus_error cret = se.CheckConsensus(etxBin, utxoHeights, blockHeight);
        if (cret != bitcoinconsensus_ERR_OK) {
            throw std::runtime_error("ERROR check consensus for TxID " + txID);
        }
    }

    const ScriptError ret = se.VerifyScript(etxBin, utxoHeights, blockHeight, consensus);
    if (ret != SCRIPT_ERR_OK) {
        throw std::runtime_error("ERROR verify script for TxID " + txID);
    }
}

// Function to trim leading and trailing whitespace
std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }

    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

// Enhanced split function to handle quotes
std::vector<std::string> split(const std::string& line, char delimiter = ',') {
    std::vector<std::string> result;
    std::string token;
    bool inQuotes = false;
    std::stringstream ss;

    for (char ch : line) {
        if (ch == '"') {
            // Toggle the inQuotes flag when encountering a double-quote
            inQuotes = !inQuotes;
            continue; // Skip the quote itself
        }

        if (ch == delimiter && !inQuotes) {
            // If we encounter a delimiter outside quotes, push the token
            result.push_back(trim(ss.str()));
            ss.str(""); // Clear the stream for the next token
            ss.clear();
        }
        else {
            // Otherwise, add the character to the token (even if it's a comma inside quotes)
            ss << ch;
        }
    }

    if (inQuotes) {
        throw std::runtime_error("missing double quote");
    }

    // Add the last token after the loop
    result.push_back(trim(ss.str()));

    return result;
}

// Function to read a CSV file
std::vector<std::vector<std::string>> parseCSV(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::vector<std::string>> data;
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return data;
    }

    // Read each line of the file
    int iLine(0);
    while (std::getline(file, line)) {
        std::vector<std::string> row;
        try {
            row = split(line);
        }
        catch (std::exception e) {
            std::cout << "ERROR bad csv line " << iLine + 1 << " error " << e.what();
            ++iLine;
            continue;
        }

        //Skip the header
        if (iLine > 0) {
            data.push_back(row);
        }

        ++iLine;
    }

    file.close();
    return data;
}

std::vector<int32_t> parseUTXOStr(const std::string& utxoStr, char delimiter = '|') {
    std::vector<int32_t> result;
    std::stringstream ss(utxoStr);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        try {
            result.push_back(std::stoi(token));
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing integer: " << e.what() << std::endl;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// This program read the csv file of format
//     ChainNet, BlockHeight, TXID, TxHexExtended
// And test to serialize and unserialize of the extended transaction
int main(int argc, char* argv[])
{
    std::string csvFilePath;
    std::string network;
    bool disableConsensus;

    // Define and parse the command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("csv-file,f", po::value<std::string>(&csvFilePath)->required(), "path to the csv file")
        ("network,n", po::value<std::string>(&network)->default_value("main"), "the name of the network type")
        ("disable-consensus,c", po::bool_switch(&disableConsensus)->default_value(false), "disable the consensus");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        po::notify(vm); // throws error if required arguments are missing
    }
    catch (po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << desc << std::endl;
        return 1;
    }

    const auto csvData = parseCSV(csvFilePath);
    const bsv::CScriptEngine se(network);
    std::chrono::duration<double> elapsed;
    size_t nbTx{ 0 };
    for (size_t i = 0; i < csvData.size();++i) {
        auto line = csvData[i];
        //std::cout << std::endl;
        //for (auto record : line) {
        //    std::cout << "["<< record << "]" << std::endl;
        //}
        //std::cout << std::endl;
        if (line.size() != 5) {
            throw std::runtime_error("bad line " + std::to_string(i) + " there are only " + std::to_string(line.size()) + " elements");
        }

        const std::string& networkInFile = line[0];
        if (networkInFile != network) {
            throw std::runtime_error("bad network" + networkInFile + " at line " + std::to_string(i) + ". Inconsistent with the global network setting : " + network);
        }

        const std::string& TxID = line[2];
        const std::string& TxHexExtended = line[3];

        std::stringstream ss(line[1]);
        int32_t blockHeight;
        if (!(ss >> blockHeight)){
            throw std::runtime_error("failed to convert to uint32 block height");
        }

        const std::string& utxoHeightsStr = line[4];
        const auto utxoVector = parseUTXOStr(utxoHeightsStr);
        std::span<const int32_t> utxoHeights(utxoVector);

        const bool consensus = !disableConsensus;
        try {
            auto start = std::chrono::high_resolution_clock::now();
            doVerifyScript(se, utxoHeights, blockHeight, TxID, TxHexExtended, consensus);
            auto end = std::chrono::high_resolution_clock::now();
            elapsed += (end - start);
            nbTx += 1;
        }
        catch (std::exception e) {
            std::cout << "ERROR test line : " << i + 1 <<" TxID : "<<TxID <<" "<< e.what() << std::endl;
        }
    }

    std::cout << "End Of Program, total csv " << csvData.size() << " lines" << std::endl;
    std::cout << "Nb Txs " << nbTx << std::endl;
    std::cout << "Processed Time " << elapsed.count() << std::endl;
    return 0;
}
