#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <memory>

struct CustomChainParams
{
    static constexpr std::string_view TERATESTNET{"teratestnet"};
    static constexpr std::string_view TERASCALINGTESTNET{"tstn"};
};

class CChainParams;

namespace bsv
{
    /**
     * Implementation factory for extensible custom CChainParams
     * If it is main, test, regtest, stn network, use the bitcoin-sv existing creator
     * Otherwise, use the registered factory here
     */
    using ChainParamsCreator = std::function<std::unique_ptr<CChainParams>()>;

    // ChainParamsRegistry implemented as a singleton
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class ChainParamsRegistry
    {
    public:
        // Get the singleton instance (declared, but not defined here!)
        static ChainParamsRegistry& Instance();

        // Definition of the singleton instance ChainParamsRegistry
        void Register(const std::string& name, ChainParamsCreator creator);

        // Create the chain param object based on its name
        std::unique_ptr<CChainParams> Create(const std::string& name) const;

        ChainParamsRegistry(const ChainParamsRegistry&) = delete;
        ChainParamsRegistry& operator=(const ChainParamsRegistry&) = delete;

    private:
        // Prevent construction from outside
        ChainParamsRegistry() = default;

        std::unordered_map<std::string, ChainParamsCreator> registry_;
    };

    // Create ChainParams global helper using its name
    std::unique_ptr<CChainParams> CreateCustomChainParams(std::string_view name);

    // Registration helper template. To register your custom MyChainParams, in your cpp file
    //
    //     static bsv::RegisterCustomChainParams<MyChainParams> myChainParamsReg("myChainParamsName");
    template <typename T>
    class RegisterCustomChainParams {
    public:
        explicit RegisterCustomChainParams(std::string_view name)
        {
            ChainParamsRegistry::Instance().Register(std::string{name},
                                                     [] { return std::make_unique<T>(); });
        }
    };
}

