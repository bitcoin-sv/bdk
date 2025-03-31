#ifndef __CHAINPARAMS_BDK_HPP__
#define __CHAINPARAMS_BDK_HPP__

#include <string>

struct CustomChainParams
{
    static const std::string TERATESTNET;        // "teratestnet"
    static const std::string TERASCALINGTESTNET; // "tstn"
};

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
     */
    void select_params(const std::string &chain);
}

#endif /* __CHAINPARAMS_BDK_HPP__ */