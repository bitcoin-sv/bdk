#include <bdkffi/bench.h>

#ifdef BDKFFI_ENABLE_BENCH

extern "C" void bdkffi_bench_noop(void)
{
    try {
    }
    catch (...) {
    }
}

extern "C" uint64_t bdkffi_bench_sum_bytes(const unsigned char* data, int data_len)
{
    try {
        if (data == nullptr || data_len <= 0) {
            return 0;
        }

        uint64_t sum = 0;
        for (int i = 0; i < data_len; ++i) {
            sum += data[i];
        }
        return sum;
    }
    catch (...) {
        return 0;
    }
}

#endif
