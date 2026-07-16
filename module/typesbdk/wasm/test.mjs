import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import createBdkModule from './bdk-core.mjs'

const vectors = JSON.parse(
  await readFile(new URL('./vectors.json', import.meta.url), 'utf8')
)

function fromHex (hex) {
  if (hex.length % 2 !== 0) throw new Error('hex must have an even length')
  const bytes = []
  for (let i = 0; i < hex.length; i += 2) bytes.push(Number.parseInt(hex.slice(i, i + 2), 16))
  return bytes
}

function toVector (Type, values) {
  const vector = new Type()
  for (const value of values) vector.push_back(value)
  return vector
}

const bdk = await createBdkModule()

for (const vector of vectors) {
  const extendedTx = toVector(bdk.VectorUInt8, fromHex(vector.extendedTx))
  const utxoHeights = toVector(bdk.VectorInt32, vector.utxoHeights)
  const customFlags = new bdk.VectorUInt32()
  try {
    const actual = bdk.VerifyScript(
      extendedTx,
      utxoHeights,
      vector.blockHeight,
      vector.consensus,
      customFlags
    )
    assert.deepEqual(actual, vector.expected, vector.name)
    console.log(`ok - ${vector.name}: domain=${actual.domain} code=${actual.code}`)
  } finally {
    extendedTx.delete()
    utxoHeights.delete()
    customFlags.delete()
  }
}
