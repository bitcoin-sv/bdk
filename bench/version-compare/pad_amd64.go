package main

// A run of NOPs in this package's own object, which the linker places ahead of
// the gobdk archive's code. Building with CGO_CFLAGS="-O2 -g -DBDK_BENCH_PAD=<n>"
// moves every function from the archive by n bytes without changing its code.

/*
#ifndef BDK_BENCH_PAD
#define BDK_BENCH_PAD 0
#endif
#define BDK_BENCH_STR2(x) #x
#define BDK_BENCH_STR(x) BDK_BENCH_STR2(x)
__asm__(".text\n.balign 64\n.globl bdk_bench_pad\nbdk_bench_pad:\n.skip " BDK_BENCH_STR(BDK_BENCH_PAD) ", 0x90\n");
static int bdk_bench_pad_size(void) { return BDK_BENCH_PAD; }
*/
import "C"

func benchPad() int { return int(C.bdk_bench_pad_size()) }
