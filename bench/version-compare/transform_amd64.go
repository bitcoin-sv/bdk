//go:build bdk14

package main

// The address of BDK's SHA-NI multi-block transform, to record where the linker put it.

/*
#include <stdint.h>
extern char _ZN16sha256_x86_shani9TransformEPjPKhm[];
static uintptr_t bdk_bench_transform_addr(void) { return (uintptr_t)_ZN16sha256_x86_shani9TransformEPjPKhm; }
*/
import "C"

func transformAddr() uint64 { return uint64(C.bdk_bench_transform_addr()) }
