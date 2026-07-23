import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'

const vectors = JSON.parse(
  await readFile(new URL('./vectors.json', import.meta.url), 'utf8')
)

function fromHex (hex) {
  if (hex.length % 2 !== 0) throw new Error('hex must have an even length')
  const bytes = new Uint8Array(hex.length / 2)
  for (let index = 0; index < bytes.length; index++) {
    bytes[index] = Number.parseInt(hex.slice(index * 2, index * 2 + 2), 16)
  }
  return bytes
}

function toVector (Type, values) {
  const vector = new Type()
  for (const value of values) vector.push_back(value)
  return vector
}

function concatenate (arrays, Type = Uint8Array) {
  const offsets = new Uint32Array(arrays.length + 1)
  let length = 0
  for (let index = 0; index < arrays.length; index++) {
    length += arrays[index].length
    offsets[index + 1] = length
  }
  const result = new Type(length)
  let position = 0
  for (const array of arrays) {
    result.set(array, position)
    position += array.length
  }
  return { values: result, offsets }
}

function readVarInt (bytes, start) {
  const prefix = bytes[start]
  if (prefix < 0xfd) return { value: prefix, end: start + 1 }
  if (prefix === 0xfd) return { value: bytes[start + 1] | (bytes[start + 2] << 8), end: start + 3 }
  throw new Error('test vector varint exceeds the supported fixture size')
}

function readUInt64LE (bytes, start) {
  let value = 0
  for (let index = 7; index >= 0; index--) value = value * 256 + bytes[start + index]
  return value
}

function rawTransactionAndSpendsFromEF (ef) {
  const rawParts = [ef.slice(0, 4)]
  let position = 10
  const inputCount = readVarInt(ef, position)
  rawParts.push(ef.slice(position, inputCount.end))
  position = inputCount.end
  const spends = []
  for (let index = 0; index < inputCount.value; index++) {
    const inputStart = position
    position += 36
    const unlockingLength = readVarInt(ef, position)
    position = unlockingLength.end + unlockingLength.value + 4
    rawParts.push(ef.slice(inputStart, position))
    const sourceSatoshis = readUInt64LE(ef, position)
    position += 8
    const lockingLength = readVarInt(ef, position)
    position = lockingLength.end
    const lockingScript = ef.slice(position, position + lockingLength.value)
    position += lockingLength.value
    spends.push({ inputIndex: index, sourceSatoshis, lockingScript })
  }
  rawParts.push(ef.slice(position))
  return { transaction: concatenate(rawParts).values, spends }
}

function assertResult (actual, expected, message) {
  assert.equal(actual.domain, expected.domain, `${message} domain`)
  assert.equal(actual.code, expected.code, `${message} code`)
}

export async function runBdkTestSuite ({
  bdk,
  createBdkModule,
  moduleOptions = {},
  label = 'BDK WASM'
}) {
  const results = []
  const hasLegacyVectors = typeof bdk.VectorUInt8 === 'function'

  for (const vector of vectors) {
    const ef = fromHex(vector.extendedTx)
    if (hasLegacyVectors) {
      const extendedTx = toVector(bdk.VectorUInt8, ef)
      const utxoHeights = toVector(bdk.VectorInt32, vector.utxoHeights)
      const customFlags = new bdk.VectorUInt32()
      try {
        const legacy = bdk.VerifyScript(
          extendedTx,
          utxoHeights,
          vector.blockHeight,
          vector.consensus,
          customFlags
        )
        assertResult(legacy, vector.expected, `${vector.name} legacy ABI`)

        const alias = extendedTx.clone()
        assert.equal(alias.isAliasOf(extendedTx), true, `${vector.name} vector aliases share storage`)
        assertResult(
          bdk.VerifyScript(alias, utxoHeights, vector.blockHeight, vector.consensus, customFlags),
          vector.expected,
          `${vector.name} aliased vector ABI`
        )
        alias.delete()
        assert.equal(alias.isDeleted(), true, `${vector.name} deleted alias is marked deleted`)
        assert.equal(extendedTx.isDeleted(), false, `${vector.name} live alias retains storage`)
        extendedTx.set(0, extendedTx.get(0))
        assertResult(
          bdk.VerifyScript(extendedTx, utxoHeights, vector.blockHeight, vector.consensus, customFlags),
          vector.expected,
          `${vector.name} vector mutation invalidates cached storage`
        )
      } finally {
        extendedTx.delete()
        utxoHeights.delete()
        customFlags.delete()
      }
    }

    const bulk = bdk.VerifyScriptArray(
      ef,
      Int32Array.from(vector.utxoHeights),
      vector.blockHeight,
      vector.consensus,
      new Uint32Array()
    )
    const network = bdk.VerifyScriptArrayNetwork(
      ef,
      Int32Array.from(vector.utxoHeights),
      vector.blockHeight,
      vector.consensus,
      new Uint32Array(),
      0
    )
    assertResult(bulk, vector.expected, `${vector.name} bulk ABI`)
    assertResult(network, vector.expected, `${vector.name} network ABI`)

    const { transaction, spends } = rawTransactionAndSpendsFromEF(ef)
    const spend = bdk.VerifySpendArray(
      transaction,
      spends[0].inputIndex,
      spends[0].lockingScript,
      spends[0].sourceSatoshis,
      vector.utxoHeights[0],
      vector.blockHeight,
      vector.consensus,
      false,
      0,
      0
    )
    assertResult(spend, vector.expected, `${vector.name} Spend ABI`)
    results.push({ vector, ef, transaction, spend: spends[0] })
    console.log(`ok - ${label} ${vector.name}: domain=${bulk.domain} code=${bulk.code}`)
  }

  const packedTransactions = concatenate(results.map(result => result.ef))
  const packedHeights = concatenate(
    results.map(result => Int32Array.from(result.vector.utxoHeights)),
    Int32Array
  )
  const emptyFlagOffsets = new Uint32Array(results.length + 1)
  const batch = bdk.VerifyScriptBatchArray(
    packedTransactions.values,
    packedTransactions.offsets,
    packedHeights.values,
    packedHeights.offsets,
    Int32Array.from(results.map(result => result.vector.blockHeight)),
    Uint8Array.from(results.map(result => result.vector.consensus ? 1 : 0)),
    new Uint32Array(),
    emptyFlagOffsets,
    0
  )
  assert.deepEqual(
    Array.from(batch),
    results.flatMap(result => [result.vector.expected.domain, result.vector.expected.code]),
    'transaction batch ABI'
  )

  const packedRawTransactions = concatenate(results.map(result => result.transaction))
  const packedLockingScripts = concatenate(results.map(result => result.spend.lockingScript))
  const spendBatch = bdk.VerifySpendBatchArray(
    packedRawTransactions.values,
    packedRawTransactions.offsets,
    Uint32Array.from(results.map(result => result.spend.inputIndex)),
    packedLockingScripts.values,
    packedLockingScripts.offsets,
    Float64Array.from(results.map(result => result.spend.sourceSatoshis)),
    Int32Array.from(results.map(result => result.vector.utxoHeights[0])),
    Int32Array.from(results.map(result => result.vector.blockHeight)),
    Uint8Array.from(results.map(result => result.vector.consensus ? 1 : 0)),
    new Uint8Array(results.length),
    new Uint32Array(results.length),
    0
  )
  assert.deepEqual(
    Array.from(spendBatch),
    results.flatMap(result => [result.vector.expected.domain, result.vector.expected.code]),
    'Spend batch ABI'
  )

  const positive = results.find(result => result.vector.expected.domain === 0)
  assert.ok(positive, 'positive fixture is present')
  for (const [networkName, networkId] of [['TeraTestNet', 4], ['Tera Scaling Test Network', 5]]) {
    const networkResult = bdk.VerifyScriptArrayNetwork(
      positive.ef,
      Int32Array.from(positive.vector.utxoHeights),
      positive.vector.blockHeight,
      positive.vector.consensus,
      new Uint32Array(),
      networkId
    )
    assertResult(networkResult, positive.vector.expected, `${networkName} network ABI`)
  }
  assert.throws(
    () => bdk.VerifyScriptArrayNetwork(
      positive.ef,
      Int32Array.from(positive.vector.utxoHeights),
      positive.vector.blockHeight,
      positive.vector.consensus,
      new Uint32Array(),
      99
    ),
    undefined,
    'unknown network IDs fail closed'
  )
  assert.throws(
    () => bdk.VerifyScriptBatchArray(
      positive.ef,
      Uint32Array.from([0]),
      Int32Array.from(positive.vector.utxoHeights),
      Uint32Array.from([0]),
      new Int32Array(),
      new Uint8Array(),
      new Uint32Array(),
      Uint32Array.from([0]),
      0
    ),
    undefined,
    'malformed empty-batch offsets fail closed'
  )
  assert.throws(
    () => bdk.VerifySpendArray(
      positive.transaction,
      positive.spend.inputIndex,
      positive.spend.lockingScript,
      Number.MAX_SAFE_INTEGER + 1,
      positive.vector.utxoHeights[0],
      positive.vector.blockHeight,
      positive.vector.consensus,
      false,
      0,
      0
    ),
    undefined,
    'unsafe source amounts fail closed'
  )

  const privateKeyOne = fromHex(
    '0000000000000000000000000000000000000000000000000000000000000001'
  )
  const privateKeyTwo = fromHex(
    '0000000000000000000000000000000000000000000000000000000000000002'
  )
  const publicKeyOne = fromHex(
    '0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798'
  )
  const publicKeyTwo = fromHex(
    '02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5'
  )
  const digest = fromHex(
    '4f3c2f0f55e2f4f08c892a2ca2c10100c33c5e7f443f3d13f86af68b4f8f25de'
  )
  const signature = bdk.SignDigest(privateKeyOne, digest)
  assert.deepEqual(
    Array.from(bdk.SignDigest(privateKeyOne, digest)),
    Array.from(signature),
    'digest signatures are deterministic'
  )
  assert.equal(
    bdk.VerifyDigest(publicKeyOne, digest, signature),
    true,
    'digest signature verifies'
  )
  const corruptedSignature = signature.slice()
  corruptedSignature[corruptedSignature.length - 1] ^= 1
  assert.equal(
    bdk.VerifyDigest(publicKeyOne, digest, corruptedSignature),
    false,
    'corrupted digest signature is rejected'
  )
  assert.deepEqual(
    Array.from(bdk.PublicKeyFromPrivate(privateKeyOne)),
    Array.from(publicKeyOne),
    'compressed public key creation'
  )
  assert.deepEqual(
    Array.from(bdk.MultiplyPublicKey(publicKeyOne, privateKeyTwo)),
    Array.from(publicKeyTwo),
    'constant-time public-key scalar multiplication'
  )
  assert.deepEqual(
    Array.from(bdk.TweakPublicKeyAdd(publicKeyOne, privateKeyOne)),
    Array.from(publicKeyTwo),
    'public-key tweak addition'
  )
  assert.deepEqual(
    Array.from(bdk.TweakPrivateKeyAdd(privateKeyOne, privateKeyOne)),
    Array.from(privateKeyTwo),
    'private-key tweak addition'
  )

  const packedPublicKeys = concatenate([publicKeyOne, publicKeyOne, publicKeyOne])
  const packedDigests = concatenate([digest, digest, digest])
  const packedSignatures = concatenate([signature, signature, corruptedSignature])
  assert.deepEqual(
    Array.from(bdk.VerifyDigestBatchArray(
      packedPublicKeys.values,
      packedPublicKeys.offsets,
      packedDigests.values,
      packedSignatures.values,
      packedSignatures.offsets
    )),
    [1, 1, 0],
    'packed digest verification returns one result byte per entry'
  )

  if (typeof bdk.ExportVerificationTables === 'function') {
    const verificationTables = bdk.ExportVerificationTables()
    assert.equal(
      verificationTables.length,
      1024 * 1024,
      'verification table snapshot contains both runtime W15 tables'
    )
    const importedBdk = await createBdkModule(moduleOptions)
    importedBdk.ImportVerificationTables(verificationTables)
    assert.equal(
      importedBdk.VerifyDigest(publicKeyOne, digest, signature),
      true,
      'a fresh instance verifies after importing the precomputed tables'
    )
    assert.throws(
      () => importedBdk.ImportVerificationTables(verificationTables.subarray(1)),
      undefined,
      'verification table imports require the exact deterministic snapshot size'
    )
    const corruptedTables = verificationTables.slice()
    corruptedTables[Math.floor(corruptedTables.length / 2)] ^= 1
    assert.throws(
      () => importedBdk.ImportVerificationTables(corruptedTables),
      undefined,
      'verification table imports reject corrupted content'
    )
  }

  console.log(`ok - ${label} compact secp256k1 primitive ABIs`)
}
