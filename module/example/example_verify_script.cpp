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

int main(int argc, char* argv[])
{
    SelectParams(CBaseChainParams::MAIN);

    const std::string uScriptHex = "48304502207ec38d0a4ef79c3a4286ba3e5a5b6ede1fa678af9242465140d78a901af9e4e0022100c26c377d44b761469cf0bdcdbf4931418f2c5a02ce6b72bbb7af52facd7228c1014104bc9eb4fe4cb53e35df7e7734c4c3cd91c6af7840be80f4a1fff283e2cd6ae8f7713cb263a4590263240e3c01ec36bc603c32281ac08773484dc69b8152e48cec";
    const std::string lScriptHex = "76a9148ac9bdc626352d16e18c26f431e834f9aae30e2888ac";
    const std::string txHex = "0100000001febe0cbd7d87d44cbd4b5adac0a5bfcdbd2b672c9113f5d74a6459a2b85569db010000008b48304502207ec38d0a4ef79c3a4286ba3e5a5b6ede1fa678af9242465140d78a901af9e4e0022100c26c377d44b761469cf0bdcdbf4931418f2c5a02ce6b72bbb7af52facd7228c1014104bc9eb4fe4cb53e35df7e7734c4c3cd91c6af7840be80f4a1fff283e2cd6ae8f7713cb263a4590263240e3c01ec36bc603c32281ac08773484dc69b8152e48cecffffffff0230424700000000001976a9148ac9bdc626352d16e18c26f431e834f9aae30e2888ac1027000000000000166a148ac9bdc626352d16e18c26f431e834f9aae30e2800000000";

    const int inIndex = 0;
    const uint64_t satoshis = 4700000;
    const uint64_t  blockHeight = 253237;

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
    std::cout << "Verification Script Return " << ret << std::endl;
    std::cout << "End Of Program "  << std::endl;
    return 0;
}
