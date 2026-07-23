/* global Module, HEAPU8, HEAP32, HEAPU32, HEAPF64 */

// Public JavaScript compatibility layer for the verifier's compact C ABI.
// Kept as post-link glue so Node, browser ESM, and UMD builds share one API.
{
  function asTypedArray (values, Type, name) {
    if (values === null || values === undefined || typeof values.length !== 'number') {
      throw new TypeError(`${name} must be an array or typed array`)
    }
    return values instanceof Type ? values : Type.from(values)
  }

  function runWithOutput (
    definitions,
    outputLength,
    OutputType,
    outputHeap,
    callback
  ) {
    const inputs = definitions.map(([values, Type, heap, name]) => ({
      typed: asTypedArray(values, Type, name),
      Type,
      heap,
      offset: 0
    }))
    let byteLength = 0
    for (const input of inputs) {
      byteLength = Math.ceil(byteLength / input.Type.BYTES_PER_ELEMENT) * input.Type.BYTES_PER_ELEMENT
      input.offset = byteLength
      byteLength += input.typed.byteLength
    }
    byteLength = Math.ceil(byteLength / OutputType.BYTES_PER_ELEMENT) * OutputType.BYTES_PER_ELEMENT
    const outputOffset = byteLength
    byteLength += outputLength * OutputType.BYTES_PER_ELEMENT
    let arena = 0
    try {
      if (byteLength !== 0) {
        arena = _malloc(byteLength)
        if (arena === 0) throw new Error(`unable to allocate ${byteLength} verifier bytes`)
      }
      const buffers = inputs.map(input => {
        const ptr = input.typed.length === 0 ? 0 : arena + input.offset
        if (ptr !== 0) input.heap().set(input.typed, ptr / input.Type.BYTES_PER_ELEMENT)
        return { ptr, length: input.typed.length }
      })
      const output = outputLength === 0 ? 0 : arena + outputOffset
      callback(buffers, output)
      return outputLength === 0
        ? new OutputType()
        : OutputType.from(outputHeap().subarray(
          output / OutputType.BYTES_PER_ELEMENT,
          output / OutputType.BYTES_PER_ELEMENT + outputLength
        ))
    } finally {
      if (arena !== 0) _free(arena)
    }
  }

  function runWithBuffers (definitions, outputLength, callback) {
    return runWithOutput(
      definitions, outputLength, Int32Array, () => HEAP32, callback
    )
  }

  function runWithByteOutput (definitions, outputLength, callback) {
    return runWithOutput(
      definitions, outputLength, Uint8Array, () => HEAPU8, callback
    )
  }

  const uint8 = values => [values, Uint8Array, () => HEAPU8, 'bytes']
  const int32 = values => [values, Int32Array, () => HEAP32, 'int32 values']
  const uint32 = values => [values, Uint32Array, () => HEAPU32, 'uint32 values']
  const float64 = values => [values, Float64Array, () => HEAPF64, 'float64 values']
  let signingPrepared = false

  function validateNetwork (network) {
    if (!Number.isInteger(network) || network < 0 || network > 5) {
      throw new RangeError('unknown BDK network')
    }
  }

  function validateOffsets (values, offsets, count, name) {
    if (offsets.length !== count + 1 || offsets[0] !== 0 || offsets[count] !== values.length) {
      throw new RangeError(`invalid ${name} offsets`)
    }
    for (let index = 0; index < count; index++) {
      if (offsets[index] > offsets[index + 1]) throw new RangeError(`non-monotonic ${name} offsets`)
    }
  }

  function exactBytes (values, size, name) {
    const bytes = asTypedArray(values, Uint8Array, name)
    if (bytes.length !== size) {
      throw new RangeError(`${name} must contain exactly ${size} bytes`)
    }
    return bytes
  }

  function publicKeyBytes (values, name = 'public key') {
    const bytes = asTypedArray(values, Uint8Array, name)
    if (bytes.length !== 33 && bytes.length !== 65) {
      throw new RangeError(`${name} must contain 33 or 65 bytes`)
    }
    return bytes
  }

  function resultObject (result) {
    return { domain: result[0], code: result[1] }
  }

  function verifyScriptArray (extendedTX, utxoHeights, blockHeight, consensus, customFlags) {
    const result = runWithBuffers(
      [uint8(extendedTX), int32(utxoHeights), uint32(customFlags)],
      2,
      ([tx, heights, flags], output) => _bdk_verify_script(
        tx.ptr, tx.length,
        heights.ptr, heights.length,
        blockHeight, consensus ? 1 : 0,
        flags.ptr, flags.length,
        0,
        output
      )
    )
    return resultObject(result)
  }

  Module.PrepareVerification = function () {
    _bdk_prepare_verification()
  }

  Module.PrepareSigning = function () {
    if (signingPrepared) return
    const random = globalThis.crypto
    if (random === undefined || typeof random.getRandomValues !== 'function') {
      throw new Error('secure random values are required to prepare the signing context')
    }
    const seed = new Uint8Array(32)
    random.getRandomValues(seed)
    let prepared = 0
    try {
      runWithByteOutput([uint8(seed)], 0, ([randomness]) => {
        prepared = _bdk_prepare_signing(randomness.ptr)
      })
    } finally {
      seed.fill(0)
    }
    if (prepared !== 1) {
      throw new Error('unable to prepare secp256k1 signing context')
    }
    signingPrepared = true
  }

  Module.VerifyScript = function (extendedTX, utxoHeights, blockHeight, consensus, customFlags) {
    return verifyScriptArray(extendedTX, utxoHeights, blockHeight, consensus, customFlags)
  }

  Module.VerifyScriptArray = Module.VerifyScript

  Module.VerifyScriptArrayNetwork = function (
    extendedTX, utxoHeights, blockHeight, consensus, customFlags, network
  ) {
    validateNetwork(network)
    const result = runWithBuffers(
      [uint8(extendedTX), int32(utxoHeights), uint32(customFlags)],
      2,
      ([tx, heights, flags], output) => _bdk_verify_script(
        tx.ptr, tx.length,
        heights.ptr, heights.length,
        blockHeight, consensus ? 1 : 0,
        flags.ptr, flags.length,
        network, output
      )
    )
    return resultObject(result)
  }

  Module.VerifyScriptBatchArray = function (
    extendedTXsValue,
    txOffsetsValue,
    utxoHeightsValue,
    heightOffsetsValue,
    blockHeightsValue,
    consensusValue,
    customFlagsValue,
    customFlagOffsetsValue,
    network
  ) {
    validateNetwork(network)
    const extendedTXs = asTypedArray(extendedTXsValue, Uint8Array, 'transactions')
    const txOffsets = asTypedArray(txOffsetsValue, Uint32Array, 'transaction offsets')
    const utxoHeights = asTypedArray(utxoHeightsValue, Int32Array, 'UTXO heights')
    const heightOffsets = asTypedArray(heightOffsetsValue, Uint32Array, 'height offsets')
    const blockHeights = asTypedArray(blockHeightsValue, Int32Array, 'block heights')
    const consensus = asTypedArray(consensusValue, Uint8Array, 'consensus values')
    const customFlags = asTypedArray(customFlagsValue, Uint32Array, 'custom flags')
    const customFlagOffsets = asTypedArray(customFlagOffsetsValue, Uint32Array, 'custom flag offsets')
    const count = blockHeights.length
    if (consensus.length !== count) throw new RangeError('batch consensus length does not match block heights')
    validateOffsets(extendedTXs, txOffsets, count, 'transaction')
    validateOffsets(utxoHeights, heightOffsets, count, 'height')
    validateOffsets(customFlags, customFlagOffsets, count, 'custom flag')

    return runWithBuffers(
      [
        uint8(extendedTXs), uint32(txOffsets), int32(utxoHeights), uint32(heightOffsets),
        int32(blockHeights), uint8(consensus), uint32(customFlags), uint32(customFlagOffsets)
      ],
      count * 2,
      ([txs, txEnds, heights, heightEnds, blocks, modes, flags, flagEnds], output) => {
        _bdk_verify_script_batch(
          txs.ptr, txs.length, txEnds.ptr,
          heights.ptr, heights.length, heightEnds.ptr,
          blocks.ptr, modes.ptr,
          flags.ptr, flags.length, flagEnds.ptr,
          count, network, output
        )
      }
    )
  }

  Module.VerifySpendArray = function (
    transaction,
    inputIndex,
    lockingScript,
    sourceSatoshis,
    utxoHeight,
    blockHeight,
    consensus,
    hasCustomFlags,
    customFlags,
    network
  ) {
    validateNetwork(network)
    if (!Number.isSafeInteger(sourceSatoshis) || sourceSatoshis < 0) {
      throw new RangeError('source satoshis must be a non-negative safe integer')
    }
    const result = runWithBuffers(
      [uint8(transaction), uint8(lockingScript)],
      2,
      ([tx, script], output) => _bdk_verify_spend(
        tx.ptr, tx.length, inputIndex,
        script.ptr, script.length, sourceSatoshis,
        utxoHeight, blockHeight, consensus ? 1 : 0,
        hasCustomFlags ? 1 : 0, customFlags,
        network, output
      )
    )
    return resultObject(result)
  }

  Module.VerifySpendBatchArray = function (
    transactionsValue,
    transactionOffsetsValue,
    inputIndicesValue,
    lockingScriptsValue,
    lockingScriptOffsetsValue,
    sourceSatoshisValue,
    utxoHeightsValue,
    blockHeightsValue,
    consensusValue,
    hasCustomFlagsValue,
    customFlagsValue,
    network
  ) {
    validateNetwork(network)
    const transactions = asTypedArray(transactionsValue, Uint8Array, 'transactions')
    const transactionOffsets = asTypedArray(transactionOffsetsValue, Uint32Array, 'transaction offsets')
    const inputIndices = asTypedArray(inputIndicesValue, Uint32Array, 'input indices')
    const lockingScripts = asTypedArray(lockingScriptsValue, Uint8Array, 'locking scripts')
    const lockingScriptOffsets = asTypedArray(lockingScriptOffsetsValue, Uint32Array, 'locking script offsets')
    const sourceSatoshis = asTypedArray(sourceSatoshisValue, Float64Array, 'source satoshis')
    const utxoHeights = asTypedArray(utxoHeightsValue, Int32Array, 'UTXO heights')
    const blockHeights = asTypedArray(blockHeightsValue, Int32Array, 'block heights')
    const consensus = asTypedArray(consensusValue, Uint8Array, 'consensus values')
    const hasCustomFlags = asTypedArray(hasCustomFlagsValue, Uint8Array, 'custom flag presence')
    const customFlags = asTypedArray(customFlagsValue, Uint32Array, 'custom flags')
    const count = inputIndices.length
    if (
      sourceSatoshis.length !== count || utxoHeights.length !== count ||
      blockHeights.length !== count || consensus.length !== count ||
      hasCustomFlags.length !== count || customFlags.length !== count
    ) throw new RangeError('spend batch metadata lengths do not match')
    validateOffsets(transactions, transactionOffsets, count, 'transaction')
    validateOffsets(lockingScripts, lockingScriptOffsets, count, 'locking script')

    return runWithBuffers(
      [
        uint8(transactions), uint32(transactionOffsets), uint32(inputIndices),
        uint8(lockingScripts), uint32(lockingScriptOffsets), float64(sourceSatoshis),
        int32(utxoHeights), int32(blockHeights), uint8(consensus),
        uint8(hasCustomFlags), uint32(customFlags)
      ],
      count * 2,
      ([txs, txEnds, inputs, scripts, scriptEnds, satoshis, heights, blocks, modes, hasFlags, flags], output) => {
        _bdk_verify_spend_batch(
          txs.ptr, txs.length, txEnds.ptr, inputs.ptr,
          scripts.ptr, scripts.length, scriptEnds.ptr,
          satoshis.ptr, heights.ptr, blocks.ptr, modes.ptr, hasFlags.ptr, flags.ptr,
          count, network, output
        )
      }
    )
  }

  Module.SignDigest = function (privateKeyValue, digestValue) {
    Module.PrepareSigning()
    const privateKey = exactBytes(privateKeyValue, 32, 'private key')
    const digest = exactBytes(digestValue, 32, 'digest')
    let signatureLength = 0
    const signature = runWithByteOutput(
      [uint8(privateKey), uint8(digest)],
      72,
      ([key, hash], output) => {
        signatureLength = _bdk_sign_digest(key.ptr, hash.ptr, output)
      }
    )
    if (signatureLength === 0 || signatureLength > signature.length) {
      throw new Error('unable to sign digest')
    }
    return signature.slice(0, signatureLength)
  }

  Module.VerifyDigest = function (publicKeyValue, digestValue, signatureValue) {
    const publicKey = publicKeyBytes(publicKeyValue)
    const digest = exactBytes(digestValue, 32, 'digest')
    const signature = asTypedArray(signatureValue, Uint8Array, 'signature')
    if (signature.length === 0 || signature.length > 72) return false
    let verified = 0
    runWithByteOutput(
      [uint8(publicKey), uint8(digest), uint8(signature)],
      0,
      ([key, hash, der]) => {
        verified = _bdk_verify_digest(
          key.ptr, key.length, hash.ptr, der.ptr, der.length
        )
      }
    )
    return verified === 1
  }

  Module.VerifyDigestBatchArray = function (
    publicKeysValue,
    publicKeyOffsetsValue,
    digestsValue,
    signaturesValue,
    signatureOffsetsValue
  ) {
    const publicKeys = asTypedArray(publicKeysValue, Uint8Array, 'public keys')
    const publicKeyOffsets = asTypedArray(
      publicKeyOffsetsValue, Uint32Array, 'public key offsets'
    )
    const digests = asTypedArray(digestsValue, Uint8Array, 'digests')
    const signatures = asTypedArray(signaturesValue, Uint8Array, 'signatures')
    const signatureOffsets = asTypedArray(
      signatureOffsetsValue, Uint32Array, 'signature offsets'
    )
    if (digests.length % 32 !== 0) {
      throw new RangeError('packed digests must contain 32 bytes per entry')
    }
    const count = digests.length / 32
    validateOffsets(publicKeys, publicKeyOffsets, count, 'public key')
    validateOffsets(signatures, signatureOffsets, count, 'signature')
    for (let index = 0; index < count; index++) {
      const keySize = publicKeyOffsets[index + 1] - publicKeyOffsets[index]
      if (keySize !== 33 && keySize !== 65) {
        throw new RangeError('each public key must contain 33 or 65 bytes')
      }
    }
    return runWithByteOutput(
      [
        uint8(publicKeys), uint32(publicKeyOffsets), uint8(digests),
        uint8(signatures), uint32(signatureOffsets)
      ],
      count,
      ([keys, keyEnds, hashes, ders, derEnds], output) => {
        _bdk_verify_digest_batch(
          keys.ptr, keys.length, keyEnds.ptr,
          hashes.ptr, hashes.length,
          ders.ptr, ders.length, derEnds.ptr,
          count, output
        )
      }
    )
  }

  function runPublicKeyOperation (definitions, callback, failureMessage) {
    let succeeded = 0
    const publicKey = runWithByteOutput(
      definitions,
      33,
      (buffers, output) => {
        succeeded = callback(buffers, output)
      }
    )
    if (succeeded !== 1) throw new Error(failureMessage)
    return publicKey
  }

  Module.PublicKeyFromPrivate = function (privateKeyValue) {
    Module.PrepareSigning()
    const privateKey = exactBytes(privateKeyValue, 32, 'private key')
    return runPublicKeyOperation(
      [uint8(privateKey)],
      ([key], output) => _bdk_public_key_from_private(key.ptr, output),
      'unable to create public key'
    )
  }

  Module.MultiplyPublicKey = function (publicKeyValue, scalarValue) {
    const publicKey = publicKeyBytes(publicKeyValue)
    const scalar = exactBytes(scalarValue, 32, 'scalar')
    return runPublicKeyOperation(
      [uint8(publicKey), uint8(scalar)],
      ([key, factor], output) => _bdk_multiply_public_key(
        key.ptr, key.length, factor.ptr, output
      ),
      'unable to multiply public key'
    )
  }

  Module.TweakPublicKeyAdd = function (publicKeyValue, tweakValue) {
    const publicKey = publicKeyBytes(publicKeyValue)
    const tweak = exactBytes(tweakValue, 32, 'tweak')
    return runPublicKeyOperation(
      [uint8(publicKey), uint8(tweak)],
      ([key, offset], output) => _bdk_tweak_public_key_add(
        key.ptr, key.length, offset.ptr, output
      ),
      'unable to tweak public key'
    )
  }

  Module.TweakPrivateKeyAdd = function (privateKeyValue, tweakValue) {
    const privateKey = exactBytes(privateKeyValue, 32, 'private key')
    const tweak = exactBytes(tweakValue, 32, 'tweak')
    let succeeded = 0
    const tweakedPrivateKey = runWithByteOutput(
      [uint8(privateKey), uint8(tweak)],
      32,
      ([key, offset], output) => {
        succeeded = _bdk_tweak_private_key_add(
          key.ptr, offset.ptr, output
        )
      }
    )
    if (succeeded !== 1) throw new Error('unable to tweak private key')
    return tweakedPrivateKey
  }
}
