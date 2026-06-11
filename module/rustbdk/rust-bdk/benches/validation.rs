use criterion::{black_box, criterion_group, criterion_main, BatchSize, Criterion, Throughput};
use rust_bdk::ValidateBatch;

mod support;

const BATCH_VALIDATE_ENTRIES: usize = 128;
const BATCH_ADD_ENTRIES: usize = 256;

pub fn bench_validate_transaction_single(c: &mut Criterion) {
    let validator = support::validator();
    let tx = support::validation_tx();

    // The inlined vector comes from module/example and is expected to validate
    // with consensus=true at this height. The first real bench run confirms
    // this and should fail fast rather than record panic-tainted timings.
    c.bench_function("bench_validate_transaction_single", |b| {
        b.iter(|| {
            black_box(
                validator.validate_transaction(
                    black_box(&tx),
                    black_box(support::UTXO_HEIGHTS),
                    black_box(support::BLOCK_HEIGHT),
                    black_box(true),
                ),
            )
            .unwrap();
        });
    });
}

pub fn bench_validate_transaction_batch(c: &mut Criterion) {
    let validator = support::validator();
    let tx = support::validation_tx();
    let batch = support::validation_batch(&tx, BATCH_VALIDATE_ENTRIES);

    // Same vector contract as the single-validation bench: confirm pass/fail at
    // the first real bench run, then measure only successful validation.
    let mut group = c.benchmark_group("bench_validate_transaction_batch");
    group.throughput(Throughput::Elements(BATCH_VALIDATE_ENTRIES as u64));
    group.bench_function("safe_api", |b| {
        b.iter(|| {
            let results = black_box(validator.validate_batch(black_box(&batch)));
            assert_eq!(results.len(), BATCH_VALIDATE_ENTRIES);
            assert!(results.into_iter().all(|result| result.is_ok()));
        });
    });
    group.finish();
}

pub fn bench_batch_add(c: &mut Criterion) {
    let tx = support::validation_tx();

    let mut group = c.benchmark_group("bench_batch_add");
    group.throughput(Throughput::Elements(BATCH_ADD_ENTRIES as u64));

    group.bench_function("owning_safe_api", |b| {
        b.iter_batched(
            || ValidateBatch::with_capacity(BATCH_ADD_ENTRIES),
            |mut batch| {
                for _ in 0..BATCH_ADD_ENTRIES {
                    batch.add(
                        black_box(&tx),
                        black_box(support::UTXO_HEIGHTS),
                        black_box(support::BLOCK_HEIGHT),
                        black_box(true),
                    );
                }
                black_box(batch.len());
            },
            BatchSize::SmallInput,
        );
    });

    group.bench_function("rust_vec_copy_baseline", |b| {
        b.iter(|| {
            let mut txs = Vec::with_capacity(BATCH_ADD_ENTRIES);
            let mut utxos = Vec::with_capacity(BATCH_ADD_ENTRIES);

            for _ in 0..BATCH_ADD_ENTRIES {
                txs.push(black_box(&tx).to_vec());
                utxos.push(black_box(support::UTXO_HEIGHTS).to_vec());
            }

            black_box((txs.len(), utxos.len()));
        });
    });

    // ValidateBatchRef is only a non-constructible placeholder today, so there
    // is no public zero-copy safe batch to put head-to-head with owning add.
    group.finish();
}

criterion_group!(
    validation_benches,
    bench_validate_transaction_single,
    bench_validate_transaction_batch,
    bench_batch_add
);
criterion_main!(validation_benches);
