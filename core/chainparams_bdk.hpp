#ifndef __CHAINPARAMS_BDK_HPP__
#define __CHAINPARAMS_BDK_HPP__

#include <string>
#include <functional>
#include <memory>

struct CustomChainParams
{
    static const std::string TERATESTNET;        // "teratestnet"
    static const std::string TERASCALINGTESTNET; // "tstn"
};

class CChainParams;

namespace bsv
{
    /**
     * Custom chain params selector that wrap the SelectParams from bsv implementation
     * and some additional chain params supported here
     * 
     * Additional chain are
     *   - Tera TestNet : teratestnet
     *   - Tera Scaling TestNet : tstn
     * 
     * @throws std::runtime_error when the chain is not supported.
     *
     * TODO : to remove once have the factory implemented
     */
    void select_params(const std::string &chain);


    /**
     * Implementation factory for extensible custom CChainParams
     * If it is main, test, regtest, stn network, use the bitcoin-sv existing creator
     * Otherwise, use the registered factory here
     */
    using ChainParamsCreator = std::function<std::unique_ptr<CChainParams>()>;

    // ChainParamsRegistry implemented as a singleton
    class ChainParamsRegistry {
    public:

        // Get the singleton instance (declared, but not defined here!)
        static ChainParamsRegistry& Instance();

        // Definition of the singleton instance ChainParamsRegistry
        void Register(const std::string& name, ChainParamsCreator creator);

        // Create the chain param object based on its name
        std::unique_ptr<CChainParams> Create(const std::string& name) const;

    private:
        // Prevent construction from outside
        ChainParamsRegistry() = default;
        ChainParamsRegistry(const ChainParamsRegistry&) = delete;
        ChainParamsRegistry& operator=(const ChainParamsRegistry&) = delete;

        std::unordered_map<std::string, ChainParamsCreator> registry_;
    };

    // Create ChainParams global helper using its name
    std::unique_ptr<CChainParams> CreateCustomChainParams(const std::string& name);

    // Registration helper template. To register your custom MyChainParams, in your cpp file
    //
    //     static bsv::RegisterCustomChainParams<MyChainParams> myChainParamsReg("myChainParamsName");
    template <typename T>
    class RegisterCustomChainParams {
    public:
        RegisterCustomChainParams(const std::string& name) {
            ChainParamsRegistry::Instance().Register(name, []() {
                return std::make_unique<T>();
                });
        }
    };
}

#endif /* __CHAINPARAMS_BDK_HPP__ */