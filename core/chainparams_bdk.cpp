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

// P2SH activation heights for the custom networks, mirroring bitcoin-sv's
// P2SH_ACTIVATION_TESTNET / P2SH_ACTIVATION_STN for the analogous networks.
constexpr int32_t P2SH_ACTIVATION_TERATESTNET{519};
constexpr int32_t P2SH_ACTIVATION_TERASCALINGTESTNET{1};

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
            consensus.BIP65Height = 581885;  // mirrors svnode CTestNetParams
            consensus.BIP66Height = 330776;  // mirrors svnode CTestNetParams
            consensus.CSVHeight = 770112;    // mirrors svnode CTestNetParams
            consensus.p2shHeight = P2SH_ACTIVATION_TERATESTNET;

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
            // svnode CStnParams leaves BIP65/BIP66/CSV unset; mirror its
            // "fast activation" intent (same as p2shHeight = 1 below) by
            // setting all three to 1.
            consensus.BIP65Height = 1;
            consensus.BIP66Height = 1;
            consensus.CSVHeight = 1;
            consensus.p2shHeight = P2SH_ACTIVATION_TERASCALINGTESTNET;

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

// Registration TeraTestNetParams and TeraScalingTestNetParams to the factory
static bsv::RegisterCustomChainParams<TeraTestNetParams> teraTestNetReg(CustomChainParams::TERATESTNET);
static bsv::RegisterCustomChainParams<TeraScalingTestNetParams> teraScalingTestNetReg( CustomChainParams::TERASCALINGTESTNET);