import assert from 'node:assert/strict'
import { performance } from 'node:perf_hooks'
import { readFile } from 'node:fs/promises'
import createBdkModule from './bdk-core.mjs'

const iterations = positiveInteger(process.argv[2] ?? '1000', 'iterations')
const samples = positiveInteger(process.argv[3] ?? '9', 'samples')
const warmupIterations = Math.max(20, Math.floor(iterations / 10))
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

const bdk = await createBdkModule()
const extendedTx = toVector(bdk.VectorUInt8, fromHex(vector.extendedTx))
const utxoHeights = toVector(bdk.VectorInt32, vector.utxoHeights)
const customFlags = new bdk.VectorUInt32()
const extendedTxArray = fromHex(vector.extendedTx)

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

  assert.deepEqual(verifyOnce(), vector.expected)
  assert.deepEqual(verifyArrayOnce(), vector.expected)

  for (const [name, operation] of [
    ['BDK WASM direct VerifyScript (legacy vectors)', verifyOnce],
    ['BDK WASM direct VerifyScriptArray', verifyArrayOnce]
  ]) {
    let resultGuard = 0
    for (let i = 0; i < warmupIterations; i++) {
      const result = operation()
      resultGuard ^= result.domain ^ result.code
    }

    const microsPerOperation = []
    for (let sample = 0; sample < samples; sample++) {
      const start = performance.now()
      for (let i = 0; i < iterations; i++) {
        const result = operation()
        resultGuard ^= result.domain ^ result.code
      }
      microsPerOperation.push(((performance.now() - start) * 1000) / iterations)
    }
    microsPerOperation.sort((a, b) => a - b)
    const median = percentile(microsPerOperation, 0.5)
    const p95 = percentile(microsPerOperation, 0.95)
    const mean = microsPerOperation.reduce((sum, value) => sum + value, 0) / samples

    console.log(JSON.stringify({
      benchmark: name,
      vector: vector.name,
      iterationsPerSample: iterations,
      samples,
      medianMicrosPerOperation: median,
      p95MicrosPerOperation: p95,
      meanMicrosPerOperation: mean,
      medianOperationsPerSecond: 1_000_000 / median,
      resultGuard
    }, null, 2))
  }
} finally {
  extendedTx.delete()
  utxoHeights.delete()
  customFlags.delete()
}
