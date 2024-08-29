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
    const std::string uScriptHex = "483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7f";
    const std::string lScriptHex = "76a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac";
    const std::string txHex = "01000000012341c8f267c6a1a407b1f09d134e42cfbdf2ebc91aff8cd4365d131380fcd580000000006b483045022100b1d382f8e5a3d125774cde860bf6286c22aaf93351fb78a2ef7262d1632f563902203688f6fecc895cb6e14fea436e34b1615d077da18e14fbdbf11395838b0181a84121034ee1f76a1460923e18bcb273c26a9b317df6644e41e49ba21dbd7c654537bc7fffffffff01a0860100000000001976a914fbb460d3176afe507a83a3b74b167e198f20f44f88ac00000000";

    const int inIndex = 0;
    const uint64_t satoshis = 100000;
    const uint64_t  blockHeight = 620538;

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

    const unsigned int flags = bsv::script_verification_flags(lScript, true);
    auto ret = bsv::verify(uScript, lScript, true, flags, tx, inIndex, satoshis);
    std::cout << "Verification Script Return " << ret << std::endl;
    std::cout << "End Of Program "  << std::endl;
    return 0;
}
