/// Use it as an example how to add a example

#include <cassert>
#include <iostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
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

// CsvDataRecord structure matching the Go version
struct CsvDataRecord {
    std::string chainNet;
    int32_t blockHeight;
    std::string txID;
    std::string txHexExtended;
    std::string utxoHeights;
    std::vector<int32_t> dataUTXOHeights;
    std::vector<uint8_t> txBinExtended;
};

std::chrono::nanoseconds doVerifyScript(const bsv::CScriptEngine& se, const CsvDataRecord& record, const bool consensus) {
    const std::span<const uint8_t> etx(record.txBinExtended.data(), record.txBinExtended.size());

    const char* begin{ reinterpret_cast<const char*>(etx.data()) };
    const char* end{ reinterpret_cast<const char*>(etx.data() + etx.size()) };
    CDataStream is(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    is>> eTX;

    ////Recover the binary
    CDataStream os(SER_NETWORK, PROTOCOL_VERSION);
    os << eTX;
    const std::vector<uint8_t> outBin(os.begin(), os.end());
    if (record.txBinExtended != outBin) {
        throw std::runtime_error("ERROR recover binary for TxID " + record.txID);
    }

    const std::string recovTxID = eTX.mtx.GetId().ToString();
    if (record.txID != recovTxID) {
        throw std::runtime_error("ERROR recover txID for TxID " + record.txID);
    }

    std::span<const int32_t> utxoHeights(record.dataUTXOHeights);
    if (consensus) {
        const bitcoinconsensus_error cret = se.CheckConsensus(record.txBinExtended, utxoHeights, record.blockHeight);
        if (cret != bitcoinconsensus_ERR_OK) {
            throw std::runtime_error("ERROR check consensus for TxID " + record.txID);
        }
    }

    auto verifyStart = std::chrono::high_resolution_clock::now();
    const ScriptError ret = se.VerifyScript(record.txBinExtended, utxoHeights, record.blockHeight, consensus);
    auto verifyEnd = std::chrono::high_resolution_clock::now();

    if (ret != SCRIPT_ERR_OK) {
        throw std::runtime_error("ERROR verify script for TxID " + record.txID);
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(verifyEnd - verifyStart);
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

// Function to read and parse CSV file into CsvDataRecord structures
std::vector<CsvDataRecord> parseCSV(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<CsvDataRecord> data;
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
            if (row.size() != 5) {
                throw std::runtime_error("bad line " + std::to_string(iLine) + " there are only " + std::to_string(row.size()) + " elements");
            }

            CsvDataRecord record;
            // Trim whitespace from all fields
            record.chainNet = trim(row[0]);
            record.txID = trim(row[2]);
            record.txHexExtended = trim(row[3]);
            record.utxoHeights = trim(row[4]);

            // Parse block height
            std::stringstream ss(row[1]);
            if (!(ss >> record.blockHeight)) {
                throw std::runtime_error("failed to convert to int32 block height at line " + std::to_string(iLine));
            }

            // Preparse binary tx (matching Go's hex.DecodeString)
            record.txBinExtended = ParseHex(record.txHexExtended);

            // Parse UTXO heights (matching Go's strings.Split and parsing)
            if (!record.utxoHeights.empty()) {
                record.dataUTXOHeights = parseUTXOStr(record.utxoHeights);
            }

            data.push_back(record);
        }

        ++iLine;
    }

    file.close();
    return data;
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
    std::chrono::nanoseconds verifyScriptElapsed(0);
    size_t nbTx{ 0 };

    // Check the network is consistent
    for (size_t i = 0; i < csvData.size(); ++i) {
        if (csvData[i].chainNet != network) {
            throw std::runtime_error("ERROR record " + std::to_string(i) + ", inconsistent network. Data " +
                                   csvData[i].chainNet + ", input " + network);
        }
    }

    const bool consensus = !disableConsensus;
    for (size_t i = 0; i < csvData.size(); ++i) {
        const auto& record = csvData[i];

        try {
            auto start = std::chrono::high_resolution_clock::now();
            auto verifyDuration = doVerifyScript(se, record, consensus);
            auto end = std::chrono::high_resolution_clock::now();
            elapsed += (end - start);
            verifyScriptElapsed += verifyDuration;
            nbTx += 1;
        }
        catch (std::exception e) {
            std::cout << "ERROR test line : " << i + 1 <<" TxID : " << record.txID << " " << e.what() << std::endl;
        }
    }

    std::cout << "End Of Program, total csv " << csvData.size() << " lines" << std::endl;
    std::cout << "Nb Txs " << nbTx << std::endl;
    std::cout << "Processed Time " << elapsed.count() << std::endl;
    std::cout << "VerifyScript Time " << std::fixed << std::setprecision(4) << verifyScriptElapsed.count() / 1e9 << " seconds" << std::endl;
    return 0;
}
