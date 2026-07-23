import assert from 'node:assert/strict'
import { performance } from 'node:perf_hooks'
import { readFile } from 'node:fs/promises'
import createBdkModule from './bdk-core.mjs'

const iterations = positiveInteger(process.argv[2] ?? '1000', 'iterations')
const samples = positiveInteger(process.argv[3] ?? '9', 'samples')
const warmupIterations = Math.max(20, Math.floor(iterations / 10))
const batchSize = 100
const [vector] = JSON.parse(
  await readFile(new URL('./vectors.json', import.meta.url), 'utf8')
)

function positiveInteger (value, name) {
  const parsed = Number.parseInt(value, 10)
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${name} must be a positive integer`)
  }
  return parsed
}

function fromHex (hex) {
  if (hex.length % 2 !== 0) throw new Error('hex must have an even length')
  const bytes = []
  for (let i = 0; i < hex.length; i += 2) bytes.push(Number.parseInt(hex.slice(i, i + 2), 16))
  return bytes
}

function toVector (Type, values) {
  const result = new Type()
  for (const value of values) result.push_back(value)
  return result
}

function percentile (sorted, fraction) {
  return sorted[Math.floor((sorted.length - 1) * fraction)]
}

function repeated (values, count, Type) {
  const result = new Type(values.length * count)
  for (let index = 0; index < count; index++) result.set(values, index * values.length)
  return result
}

const bdk = await createBdkModule()
const extendedTx = toVector(bdk.VectorUInt8, fromHex(vector.extendedTx))
const utxoHeights = toVector(bdk.VectorInt32, vector.utxoHeights)
const customFlags = new bdk.VectorUInt32()
const extendedTxArray = fromHex(vector.extendedTx)
const batchTransactions = repeated(extendedTxArray, batchSize, Uint8Array)
const batchTransactionOffsets = Uint32Array.from(
  { length: batchSize + 1 },
  (_, index) => index * extendedTxArray.length
)
const batchHeights = repeated(Int32Array.from(vector.utxoHeights), batchSize, Int32Array)
const batchHeightOffsets = Uint32Array.from(
  { length: batchSize + 1 },
  (_, index) => index * vector.utxoHeights.length
)
const batchBlockHeights = new Int32Array(batchSize).fill(vector.blockHeight)
const batchConsensus = new Uint8Array(batchSize).fill(vector.consensus ? 1 : 0)
const batchFlagOffsets = new Uint32Array(batchSize + 1)

try {
  const verifyOnce = () => bdk.VerifyScript(
    extendedTx,
    utxoHeights,
    vector.blockHeight,
    vector.consensus,
    customFlags
  )

  const verifyArrayOnce = () => bdk.VerifyScriptArray(
    extendedTxArray,
    vector.utxoHeights,
    vector.blockHeight,
    vector.consensus,
    []
  )

  const verifyBatchOnce = () => bdk.VerifyScriptBatchArray(
    batchTransactions,
    batchTransactionOffsets,
    batchHeights,
    batchHeightOffsets,
    batchBlockHeights,
    batchConsensus,
    new Uint32Array(),
    batchFlagOffsets,
    0
  )

  assert.deepEqual(verifyOnce(), vector.expected)
  assert.deepEqual(verifyArrayOnce(), vector.expected)
  assert.deepEqual(
    Array.from(verifyBatchOnce()),
    Array.from({ length: batchSize }, () => [vector.expected.domain, vector.expected.code]).flat()
  )

  for (const [name, operation, operationScale] of [
    ['BDK WASM direct VerifyScript (legacy vectors)', verifyOnce, 1],
    ['BDK WASM direct VerifyScriptArray', verifyArrayOnce, 1],
    [`BDK WASM direct VerifyScriptBatchArray (${batchSize} inputs)`, verifyBatchOnce, batchSize]
  ]) {
    const operationIterations = Math.max(1, Math.floor(iterations / operationScale))
    let resultGuard = 0
    for (let i = 0; i < Math.max(2, Math.floor(warmupIterations / operationScale)); i++) {
      const result = operation()
      resultGuard ^= result.domain === undefined ? result[0] : (result.domain ^ result.code)
    }

    const microsPerInput = []
    for (let sample = 0; sample < samples; sample++) {
      const start = performance.now()
      for (let i = 0; i < operationIterations; i++) {
        const result = operation()
        resultGuard ^= result.domain === undefined ? result[0] : (result.domain ^ result.code)
      }
      microsPerInput.push(
        ((performance.now() - start) * 1000) / (operationIterations * operationScale)
      )
    }
    microsPerInput.sort((a, b) => a - b)
    const median = percentile(microsPerInput, 0.5)
    const p95 = percentile(microsPerInput, 0.95)
    const mean = microsPerInput.reduce((sum, value) => sum + value, 0) / samples

    console.log(JSON.stringify({
      benchmark: name,
      vector: vector.name,
      inputsPerSample: operationIterations * operationScale,
      samples,
      medianMicrosPerInput: median,
      p95MicrosPerInput: p95,
      meanMicrosPerInput: mean,
      medianInputsPerSecond: 1_000_000 / median,
      resultGuard
    }, null, 2))
  }
} finally {
  extendedTx.delete()
  utxoHeights.delete()
  customFlags.delete()
}
