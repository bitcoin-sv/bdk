/// Use it as an example how to add a example

#include <cassert>
#include <iostream>
#include <sstream>

#include "base58.h"
#include "chainparams.h"
#include "config.h"
#include "core_io.h"
#include "key.h"
#include "script/script_num.h"
#include "univalue/include/univalue.h"

#include "interpreter_bdk.hpp"

#include "assembler.h"
#include "utilstrencodings.h"

#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace po = boost::program_options;
namespace pt = boost::property_tree;

ScriptError runExampleVerification() {

    const std::string uScriptHex = "";
    const std::string lScriptHex = "";
    const std::string txHex = "";

    const int inIndex = 0;
    const uint64_t satoshis = 4338;
    const uint64_t  blockHeight = 720808;

    const std::vector<uint8_t> uScriptBin = ParseHex(uScriptHex);
    const std::vector<uint8_t> lScriptBin = ParseHex(lScriptHex);
    const std::vector<uint8_t> txBin = ParseHex(txHex);

    const std::span<const uint8_t> uScript(uScriptBin);
    const std::span<const uint8_t> lScript(lScriptBin.data(), lScriptBin.size());
    const std::span<const uint8_t> tx(txBin.data(), txBin.size());

    auto uScriptASM = bsv::to_asm(uScript);
    auto lScriptASM = bsv::to_asm(lScript);
    std::cout << "Unlocking Script : " << std::endl << uScriptASM << std::endl << std::endl;
    std::cout << "Locking Script : " << std::endl << lScriptASM << std::endl << std::endl;

    //const unsigned int flags = bsv::script_verification_flags(lScript, false);
    const uint32_t flags = bsv::script_verification_flags_v2(lScript, blockHeight);

    auto ret = bsv::verify(uScript, lScript, true, flags, tx, inIndex, satoshis);
    return ret;
}

int main(int argc, char* argv[])
{

    std::string jsonFilePath;
    std::string network{ CBaseChainParams::MAIN };

    // Define and parse the command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("json-file,j", po::value<std::string>(&jsonFilePath)->required(), "path to the JSON file")
        ("network,n", po::value<std::string>(&network), "Network type : main, test, regtest, stn");

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

    // Read and parse the JSON file
    SelectParams(network);
    pt::ptree root;
    try {
        std::ifstream jsonFile(jsonFilePath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Could not open file: " + jsonFilePath);
        }

        // Parse the JSON into the property tree
        pt::read_json(jsonFile, root);

        // Close the file
        jsonFile.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file from "<< jsonFilePath  <<". Error : " << e.what() << std::endl;
        return 1;
    }

    /*
     * The JSON must be of this format
        {
            "uScript": "",
            "lScript": "",
            "txBytes": "",
            "flags": 1394182,
            "input": 0,
            "satoshis": 4338,
            "blockHeight": 720808
        }
    */
    ScriptError ret;
    try {

        const std::string uScriptHex = root.get<std::string>("uScript");
        const std::string lScriptHex = root.get<std::string>("lScript");
        const std::string txHex = root.get<std::string>("txBytes");

        const int inIndex = root.get<int>("input");
        const uint64_t satoshis = root.get<uint64_t>("satoshis");;
        const uint64_t  blockHeight = root.get<uint64_t>("blockHeight");;

        const std::vector<uint8_t> uScriptBin = ParseHex(uScriptHex);
        const std::vector<uint8_t> lScriptBin = ParseHex(lScriptHex);
        const std::vector<uint8_t> txBin = ParseHex(txHex);

        const std::span<const uint8_t> uScript(uScriptBin);
        const std::span<const uint8_t> lScript(lScriptBin.data(), lScriptBin.size());
        const std::span<const uint8_t> tx(txBin.data(), txBin.size());

        auto uScriptASM = bsv::to_asm(uScript);
        auto lScriptASM = bsv::to_asm(lScript);
        //std::cout << "Unlocking Script : " << std::endl << uScriptASM << std::endl << std::endl;
        //std::cout << "Locking Script : " << std::endl << lScriptASM << std::endl << std::endl;

        //const unsigned int flags = bsv::script_verification_flags(lScript, false);
        uint32_t flags = bsv::script_verification_flags_v2(lScript, blockHeight);

        flags |= SCRIPT_UTXO_AFTER_GENESIS;// HACKING HERE, need to find real solution

        ret = bsv::verify(uScript, lScript, true, flags, tx, inIndex, satoshis);
    }
    catch (const pt::ptree_bad_path& e) {
        std::cerr << "Error parsing JSON : " << e.what() << std::endl;
    }

    //ret = runExampleVerification();
    std::cout << "Verification Script Return " << ret << std::endl;
    std::cout << "End Of Program " << std::endl;

    return 0;
}
