#include <iostream>
#include <sstream>

#include "config.h"
#include "script/interpreter.h"
#include "taskcancellation.h"
#include "interpreter_bdk.hpp"

using namespace std;

int main(int argc, char* argv[])
{
    constexpr array<uint8_t, 6> direct{OP_2, OP_2, OP_ADD, OP_4, OP_EQUAL};
    const Config& config = GlobalConfig::GetConfig();
    auto source = task::CCancellationSource::Make();
    LimitedStack stack(UINT32_MAX);
    const auto res = EvalScript(config,
                                true,
                                source->GetToken(),
                                stack,
                                CScript(direct.data(), direct.data() + direct.size()),
                                0,
                                BaseSignatureChecker{});

    const auto finalErr = bsv::get_raw_eval_script(res);

    cout << "Result ScriptError: " << finalErr << '\n';
}
