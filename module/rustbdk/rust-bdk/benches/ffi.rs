use criterion::{Criterion, Throughput, black_box, criterion_group, criterion_main};
use rust_bdk::bench_support::{ffi_noop, sum_bytes};

mod support;

pub fn bench_ffi_noop(c: &mut Criterion) {
    c.bench_function("bench_ffi_noop", |b| {
        b.iter(|| ffi_noop());
    });
}

pub fn bench_sum_bytes_1k(c: &mut Criterion) {
    bench_sum_bytes_len(c, "bench_sum_bytes_1k", 1_000);
}

pub fn bench_sum_bytes_10k(c: &mut Criterion) {
    bench_sum_bytes_len(c, "bench_sum_bytes_10k", 10_000);
}

pub fn bench_sum_bytes_100k(c: &mut Criterion) {
    bench_sum_bytes_len(c, "bench_sum_bytes_100k", 100_000);
}

fn bench_sum_bytes_len(c: &mut Criterion, name: &str, len: usize) {
    let bytes = support::deterministic_bytes(len);

    let mut group = c.benchmark_group(name);
    group.throughput(Throughput::Bytes(len as u64));
    group.bench_function(name, |b| {
        b.iter(|| black_box(sum_bytes(black_box(&bytes))));
    });
    group.finish();
}

criterion_group!(
    ffi_benches,
    bench_ffi_noop,
    bench_sum_bytes_1k,
    bench_sum_bytes_10k,
    bench_sum_bytes_100k
);
criterion_main!(ffi_benches);
