#ifndef BDKFFI_BENCH_H
#define BDKFFI_BENCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BDKFFI_ENABLE_BENCH
void bdkffi_bench_noop(void);
uint64_t bdkffi_bench_sum_bytes(const unsigned char* data, int data_len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_BENCH_H */
