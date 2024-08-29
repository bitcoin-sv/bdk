#include <array>
#include <iostream>
#include <sstream>

#include "assembler.h"
#include "interpreter_bdk.hpp"
#include "script/opcodes.h"

using namespace std;

int main(int argc, char* argv[])
{
    try
    {
        const array<uint8_t, 7> script{OP_1, OP_1, OP_ADD, OP_2, OP_EQUAL};
        bool concensus{true};
        unsigned int flags{};
        auto status{bsv::execute(script, concensus, flags)};

        if(status == SCRIPT_ERR_OK)
            cout << "Sucessful script execution" << endl;

        const string script_string{"0x51 0x51 0x93 0x52 0x87"};
        status = bsv::execute(script_string, concensus, flags);
        if(status == SCRIPT_ERR_OK)
            cout << "Successful script execution from string" << endl;

        const string script_hash_example{
            "'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 "
            "0xca139bc10c2f660da42666f72e89a225936fc60f193c1"
            "61124a672050c434671 0x88"};
        status = bsv::execute(script_hash_example, concensus, flags);
        if(status == SCRIPT_ERR_OK)
            cout << "Successful script execution from string with a hash "
                    "value\n";

        const string script_hash{
            "'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 "
            "0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671"
            " "
            "OP_EQUALVERIFY"};
        status = bsv::execute(script_hash, concensus, flags);
        if(status == SCRIPT_ERR_OK)
            cout << "Successful script execution from string with a hash value "
                    "& "
                    "op codes\n";
    }
    catch(const exception& e)
    {
        cout << "Error: " << e.what() << '\n';
    }
    catch(...)
    {
        cout << "unhandled exception";
    }
}
