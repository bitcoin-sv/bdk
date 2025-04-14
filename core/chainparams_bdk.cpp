#include <chainparams_bdk.hpp>
#include <chainparams.h>
#include <policy/policy.h>

#include <unordered_map>
#include <stdexcept>


namespace bsv
{
    using RegistryMap = std::unordered_map<std::string, ChainParamsCreator>;

    ChainParamsRegistry& ChainParamsRegistry::Instance() {
        static ChainParamsRegistry instance;
        return instance;
    }

    // Creator registration
    void ChainParamsRegistry::Register(const std::string& name, ChainParamsCreator creator) {
        registry_[name] = std::move(creator);
    }

    std::unique_ptr<CChainParams> ChainParamsRegistry::Create(const std::string& chainName) const {
        // If standard chain, use the bitcoin-sv creator
        if (chainName == CBaseChainParams::MAIN
            || chainName == CBaseChainParams::TESTNET
            || chainName == CBaseChainParams::REGTEST
            || chainName == CBaseChainParams::STN
            ) {
            return CreateChainParams(chainName);
        }

        // Else, custom chain, lookup the registry
        auto it = registry_.find(chainName);
        if (it != registry_.end()) {
            return (it->second)();
        }

        // If finally not found in registry, throw runtime error
        throw std::runtime_error("Unknown chain name: " + chainName);
    }

    std::unique_ptr<CChainParams> CreateCustomChainParams(const std::string& name) {
        return ChainParamsRegistry::Instance().Create(name);
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

const std::string CustomChainParams::TERATESTNET = "teratestnet";
const std::string CustomChainParams::TERASCALINGTESTNET = "tstn";

/**
 * TeraTestNetParams
 * 
 * Definitions here are copied from teratestnet in teranode code
 * Fields marked ND means they are Not Defined in teratestnet
 */
class TeraTestNetParams : public CChainParams {
    public:
        TeraTestNetParams() {
            strNetworkID = CustomChainParams::TERATESTNET;
            consensus.BIP34Height = 100000000;
            consensus.BIP34Hash = uint256S("0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8"); // ND
            // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
            consensus.BIP65Height = 1351;
            // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
            consensus.BIP66Height = 1251;
            // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
            consensus.CSVHeight = 0;
    
            // August 1, 2017 hard fork
            consensus.uahfHeight = 0;
    
            // November 13, 2017 hard fork
            consensus.daaHeight = 0;
    
            // February 2020, Genesis Upgrade
            consensus.genesisHeight = 1;
    
            // TBD, Chronicle Upgrade
            consensus.chronicleHeight = 1; // ND
        }
};

/**
 * TeraScalingTestNetParams
 * 
 * Definitions here are copied from teratestnet in teranode code
 * Fields marked ND means they are Not Defined in teratestnet
 */
class TeraScalingTestNetParams : public CChainParams {
    public:
    TeraScalingTestNetParams() {
            strNetworkID = CustomChainParams::TERASCALINGTESTNET;
            consensus.BIP34Height = 100000000;
            consensus.BIP34Hash = uint256S("0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8"); // ND
            // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
            consensus.BIP65Height = 1351;
            // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
            consensus.BIP66Height = 1251;
            // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
            consensus.CSVHeight = 0;

            // August 1, 2017 hard fork
            consensus.uahfHeight = 0;
    
            // November 13, 2017 hard fork
            consensus.daaHeight = 0;
    
            // February 2020, Genesis Upgrade
            consensus.genesisHeight = 1;
    
            // TBD, Chronicle Upgrade
            consensus.chronicleHeight = 1; // ND
        }
};

/**
 * select_params is the bdk specific of bsv SelectParam
 * It is a wrapper or 'override' version of SelectParam
 * 
 * It hander the the extra custom chain params that are listed above
 * 
 * TODO :
 *   As we don't have direct write access to the variable `globalChainParams`
 *   So we can not set custom chain params. For now, we just map any custom
 *   chain param to the regtest params
 * 
 *   Which means the script validation behavior of any custom chain will be
 *   as they are regtest mode
 */
void bsv::select_params(const std::string &chain)
{
    if (chain == CustomChainParams::TERATESTNET) {
        //globalChainParams = std::unique_ptr<CChainParams>(new TeraTestNetParams());
        SelectParams("regtest");
    }
    else if (chain == CustomChainParams::TERASCALINGTESTNET) {
        //globalChainParams = std::unique_ptr<CChainParams>(new TeraScalingTestNetParams());
        SelectParams("regtest");
    } else {
        SelectParams(chain);        
    }
}

// Registration TeraTestNetParams and TeraScalingTestNetParams to the factory
static bsv::RegisterCustomChainParams<TeraTestNetParams> teraTestNetReg(CustomChainParams::TERATESTNET);
static bsv::RegisterCustomChainParams<TeraScalingTestNetParams> teraScalingTestNetReg( CustomChainParams::TERASCALINGTESTNET);