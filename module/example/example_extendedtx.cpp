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
#include "extendedTx.hpp"

#include "assembler.h"
#include "utilstrencodings.h"

#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace po = boost::program_options;
namespace pt = boost::property_tree;

const std::string TxID = "60e3920864b99579668161d1d9a4b76bf0fd49ae8fda5ea3bed31cdbacb4c7fc";
const std::string TxHex = "010000000231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffffdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";
const std::string TxHexExtended = "010000000000000000ef0231d47120addc0d5c301942d6eb23d8bff4bfff61fd92ad9d373c497d2cf43a7d000000008c493046022100c987a6cbe33fa15d9006a9cbe8d3dba8a07aa71b8875e517ba96f906216c5b8e022100b905193089b4fe0679dda92619227d703342e268e41b61fc2199d707dbc0802e014104148012523fb728b260fe2cca2320f64145acb8f5d96b6b04fe3f59eca93f8f2c37269189d578510b33146c5d399e2f6e08dcec2ec01547562204036513782a11ffffffff006fe0d6010000001976a914fa479b2c6d09bfbff12668fa1d20a0b2bd73494088acdc316d815277ef68f92a05d51986fa7518e650fc3b3419b8637190ec14b41a33000000008a47304402200fb302faa9c2fa11e42459fac4c37ed9a7d5145e9768f374dfc5a589a487840302206230a7f00b98c1277ca085d9b8eea1a021814bb6fc5145da880cb7ec3f5de2900141049ff521df5486529bdc03d98f530c89fc2c68c4fc333dae7721f43f24c106177a2fbe8410344d54e2ec860f313733e08fdf910d1c1ec13d1aff38a34377c843a3ffffffff4075ca52000000001976a914d1d323d7e48d4b3fbe8e4ae160b0f6f1dbbabdb288ac0200b85fe6010000001976a9149d2ca1a6a7af6112078a5991983013e84ba5ef3588ac402c4b43000000001976a9148933ffc1e779f20d603afd6c19ba50081a2881d288ac00000000";


////////////////////////////////////////////////////////////////////////////////

void TestTxExtended() {
    const std::vector<uint8_t> etxBin = ParseHex(TxHexExtended);
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
    if (TxHexExtended == recovTxHexExtended) {
        std::cout << "Recover hex string OK" << std::endl;
    }
    else {
        std::cout << "Recover hex string Failed" << std::endl << std::endl
            << "Origin  Hex" << std::endl << TxHexExtended << std::endl << std::endl
            << "Recover Hex" << std::endl << recovTxHexExtended << std::endl << std::endl;
    }

    std::cout << "End Of serialization extended tx : " << eTX.mtx.GetId().ToString() << std::endl;
}

////////////////////////////////////////////////////////////////////////////////

void TestTx() {
    const std::vector<uint8_t> txBin = ParseHex(TxHex);
    const std::span<const uint8_t> tx(txBin.data(), txBin.size());

    const char* begin{ reinterpret_cast<const char*>(tx.data()) };
    const char* end{ reinterpret_cast<const char*>(tx.data() + tx.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction mtx;
    tx_stream >> mtx;
    std::cout << "End Of serialization tx : " << mtx.GetId().ToString() << std::endl;
}

int main(int argc, char* argv[])
{
    TestTx();
    TestTxExtended();
    std::cout << "End Of Program " << std::endl;
    return 0;
}
