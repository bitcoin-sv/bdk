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

// p2shHeight is not defined in teranode chaincfg; values mirror svnode
// P2SH_ACTIVATION_TESTNET / P2SH_ACTIVATION_STN for the analogous networks.
constexpr int32_t P2SH_ACTIVATION_TERATESTNET{519};
constexpr int32_t P2SH_ACTIVATION_TERASCALINGTESTNET{1};

/**
 * TeraTestNetParams
 *
 * Each field's trailing comment indicates its source:
 *   "from teranode ..."           → copied verbatim from teranode TeraTestNetParams.
 *   "from svnode CTestNetParams"  → field not defined in teranode; inspired by svnode testnet.
 */
class TeraTestNetParams : public CChainParams {
    public:
        TeraTestNetParams() {
            strNetworkID = CustomChainParams::TERATESTNET;

            consensus.BIP34Height = 0;                                       // from teranode
            consensus.BIP34Hash = uint256S(
                "0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8");  // from svnode CTestNetParams
            consensus.BIP65Height = 0;                                       // from teranode
            consensus.BIP66Height = 0;                                       // from teranode
            consensus.CSVHeight = 0;                                         // from teranode
            consensus.p2shHeight = P2SH_ACTIVATION_TERATESTNET;              // from svnode CTestNetParams (519)
            consensus.uahfHeight = 0;                                        // from teranode UahfForkHeight
            consensus.daaHeight = 0;                                         // from teranode DaaForkHeight
            consensus.genesisHeight = 1;                                     // from teranode GenesisActivationHeight
            consensus.chronicleHeight = 2;                                   // from teranode ChronicleActivationHeight
        }
};

/**
 * TeraScalingTestNetParams
 *
 * Each field's trailing comment indicates its source:
 *   "from teranode ..."        → copied verbatim from teranode TeraScalingTestNetParams.
 *   "from svnode CStnParams"   → field not defined in teranode; inspired by svnode STN.
 */
class TeraScalingTestNetParams : public CChainParams {
    public:
    TeraScalingTestNetParams() {
            strNetworkID = CustomChainParams::TERASCALINGTESTNET;

            consensus.BIP34Height = 0;                                       // from teranode
            consensus.BIP34Hash = uint256();                                 // from svnode CStnParams (zero hash)
            consensus.BIP65Height = 0;                                       // from teranode
            consensus.BIP66Height = 0;                                       // from teranode
            consensus.CSVHeight = 0;                                         // from teranode
            consensus.p2shHeight = P2SH_ACTIVATION_TERASCALINGTESTNET;       // from svnode CStnParams (1)
            consensus.uahfHeight = 0;                                        // from teranode UahfForkHeight
            consensus.daaHeight = 0;                                         // from teranode DaaForkHeight
            consensus.genesisHeight = 1;                                     // from teranode GenesisActivationHeight
            consensus.chronicleHeight = 2;                                   // from teranode ChronicleActivationHeight
        }
};

// Registration TeraTestNetParams and TeraScalingTestNetParams to the factory
static bsv::RegisterCustomChainParams<TeraTestNetParams> teraTestNetReg(CustomChainParams::TERATESTNET);
static bsv::RegisterCustomChainParams<TeraScalingTestNetParams> teraScalingTestNetReg( CustomChainParams::TERASCALINGTESTNET);