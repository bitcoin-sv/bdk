/* global Module, HEAPU8, HEAP32, HEAPU32, HEAPF64 */

// Public JavaScript compatibility layer for the verifier's compact C ABI.
// Kept as post-link glue so Node, browser ESM, and UMD builds share one API.
{
  const vectorHandle = Symbol('BDK verifier vector')
  let singleResultPointer = 0

  class VectorHandle {
    clone () {
      const handle = checkedHandle(this)
      const clone = Object.create(Object.getPrototypeOf(this))
      handle.store.references++
      clone[vectorHandle] = { store: handle.store, deleted: false }
      return clone
    }

    delete () {
      const handle = this[vectorHandle]
      if (handle === undefined || handle.deleted) return
      handle.deleted = true
      handle.store.references--
      if (handle.store.references === 0 && handle.store.pointer !== 0) {
        Module._free(handle.store.pointer)
        handle.store.pointer = 0
        handle.store.ptr = 0
        handle.store.capacity = 0
      }
    }

    deleteLater () {
      this.delete()
      return this
    }

    isAliasOf (other) {
      const handle = checkedHandle(this)
      const otherHandle = checkedHandle(other)
      return handle.store === otherHandle.store
    }

    isDeleted () {
      return this[vectorHandle]?.deleted !== false
    }
  }

  function checkedHandle (value) {
    const handle = value?.[vectorHandle]
    if (handle === undefined || handle.deleted) {
      throw new Error('Cannot pass deleted verifier vector')
    }
    return handle
  }

  function makeVectorType (name, Type) {
    const Vector = class extends VectorHandle {
      constructor () {
        super()
        this[vectorHandle] = {
          store: {
            Type,
            values: [],
            pointer: 0,
            ptr: 0,
            length: 0,
            capacity: 0,
            dirty: false,
            references: 1
          },
          deleted: false
        }
      }

      get (index) {
        return checkedHandle(this).store.values[index]
      }

      push_back (value) {
        const store = checkedHandle(this).store
        store.values.push(Type.of(value)[0])
        store.dirty = true
      }

      resize (size, value = 0) {
        if (!Number.isSafeInteger(size) || size < 0) throw new RangeError('invalid vector size')
        const store = checkedHandle(this).store
        const converted = Type.of(value)[0]
        while (store.values.length < size) store.values.push(converted)
        store.values.length = size
        store.dirty = true
      }

      set (index, value) {
        const store = checkedHandle(this).store
        if (index >= 0 && index < store.values.length) {
          store.values[index] = Type.of(value)[0]
          store.dirty = true
        }
        return true
      }

      size () {
        return checkedHandle(this).store.values.length
      }
    }
    Object.defineProperty(Vector, 'name', { value: name })
    return Vector
  }

  function asTypedArray (values, Type, name) {
    const vector = values?.[vectorHandle]
    if (vector !== undefined) {
      const handle = checkedHandle(values)
      if (handle.store.Type !== Type) throw new TypeError(`${name} has the wrong vector type`)
      return Type.from(handle.store.values)
    }
    if (values === null || values === undefined || typeof values.length !== 'number') {
      throw new TypeError(`${name} must be an array or typed array`)
    }
    return values instanceof Type ? values : Type.from(values)
  }

  function materializeVector (store, heap, name) {
    const length = store.values.length
    store.length = length
    if (length === 0) {
      store.ptr = 0
      store.dirty = false
      return store
    }
    if (length > 0xffffffff / store.Type.BYTES_PER_ELEMENT) {
      throw new RangeError(`${name} is too large`)
    }
    const byteLength = length * store.Type.BYTES_PER_ELEMENT
    if (store.capacity < byteLength) {
      const pointer = Module._malloc(byteLength)
      if (pointer === 0) throw new Error(`unable to allocate ${byteLength} verifier bytes`)
      if (store.pointer !== 0) Module._free(store.pointer)
      store.pointer = pointer
      store.capacity = byteLength
      store.dirty = true
    }
    store.ptr = store.pointer
    if (store.dirty) {
      heap().set(store.Type.from(store.values), store.pointer / store.Type.BYTES_PER_ELEMENT)
      store.dirty = false
    }
    return store
  }

  function runWithBuffers (definitions, outputLength, callback) {
    const inputs = definitions.map(([values, Type, heap, name]) => {
      const vector = values?.[vectorHandle]
      if (vector === undefined) {
        return { typed: asTypedArray(values, Type, name), Type, heap, offset: 0 }
      }
      const store = checkedHandle(values).store
      if (store.Type !== Type) throw new TypeError(`${name} has the wrong vector type`)
      return { external: materializeVector(store, heap, name), Type, heap, offset: 0 }
    })
    let byteLength = 0
    for (const input of inputs) {
      if (input.external !== undefined) continue
      byteLength = Math.ceil(byteLength / input.Type.BYTES_PER_ELEMENT) * input.Type.BYTES_PER_ELEMENT
      input.offset = byteLength
      byteLength += input.typed.byteLength
    }
    byteLength = Math.ceil(byteLength / Int32Array.BYTES_PER_ELEMENT) * Int32Array.BYTES_PER_ELEMENT
    const outputOffset = byteLength
    byteLength += outputLength * Int32Array.BYTES_PER_ELEMENT
    let arena = 0
    try {
      if (byteLength !== 0) {
        arena = Module._malloc(byteLength)
        if (arena === 0) throw new Error(`unable to allocate ${byteLength} verifier bytes`)
      }
      const buffers = inputs.map(input => {
        if (input.external !== undefined) return input.external
        const ptr = input.typed.length === 0 ? 0 : arena + input.offset
        if (ptr !== 0) input.heap().set(input.typed, ptr / input.Type.BYTES_PER_ELEMENT)
        return { ptr, length: input.typed.length }
      })
      const output = outputLength === 0 ? 0 : arena + outputOffset
      callback(buffers, output)
      return outputLength === 0
        ? new Int32Array()
        : Int32Array.from(HEAP32.subarray(output >> 2, (output >> 2) + outputLength))
    } finally {
      if (arena !== 0) Module._free(arena)
    }
  }

  const uint8 = values => [values, Uint8Array, () => HEAPU8, 'bytes']
  const int32 = values => [values, Int32Array, () => HEAP32, 'int32 values']
  const uint32 = values => [values, Uint32Array, () => HEAPU32, 'uint32 values']
  const float64 = values => [values, Float64Array, () => HEAPF64, 'float64 values']

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

  function resultObject (result) {
    return { domain: result[0], code: result[1] }
  }

  function reusableResultPointer () {
    if (singleResultPointer === 0) {
      singleResultPointer = Module._malloc(2 * Int32Array.BYTES_PER_ELEMENT)
      if (singleResultPointer === 0) throw new Error('unable to allocate verifier result')
    }
    return singleResultPointer
  }

  function verifyScriptVectors (
    txHandle,
    heightHandle,
    flagHandle,
    blockHeight,
    consensus
  ) {
    const tx = txHandle.store
    const heights = heightHandle.store
    const flags = flagHandle.store
    if (tx.Type !== Uint8Array || heights.Type !== Int32Array || flags.Type !== Uint32Array) {
      throw new TypeError('verifier vector has the wrong element type')
    }
    if (tx.dirty) materializeVector(tx, () => HEAPU8, 'bytes')
    if (heights.dirty) materializeVector(heights, () => HEAP32, 'int32 values')
    if (flags.dirty) materializeVector(flags, () => HEAPU32, 'uint32 values')
    const output = reusableResultPointer()
    Module._bdk_verify_script_main(
      tx.ptr, tx.length,
      heights.ptr, heights.length,
      blockHeight, consensus ? 1 : 0,
      flags.ptr, flags.length,
      output
    )
    return { domain: HEAP32[output >> 2], code: HEAP32[(output >> 2) + 1] }
  }

  function verifyScriptMain (extendedTX, utxoHeights, blockHeight, consensus, customFlags) {
    const txHandle = extendedTX?.[vectorHandle]
    const heightHandle = utxoHeights?.[vectorHandle]
    const flagHandle = customFlags?.[vectorHandle]
    if (txHandle !== undefined && heightHandle !== undefined && flagHandle !== undefined) {
      if (txHandle.deleted || heightHandle.deleted || flagHandle.deleted) {
        throw new Error('Cannot pass deleted verifier vector')
      }
      return verifyScriptVectors(
        txHandle,
        heightHandle,
        flagHandle,
        blockHeight,
        consensus
      )
    }
    const result = runWithBuffers(
      [uint8(extendedTX), int32(utxoHeights), uint32(customFlags)],
      2,
      ([tx, heights, flags], output) => Module._bdk_verify_script_main(
        tx.ptr, tx.length,
        heights.ptr, heights.length,
        blockHeight, consensus ? 1 : 0,
        flags.ptr, flags.length,
        output
      )
    )
    return resultObject(result)
  }

  Module.VectorUInt8 = makeVectorType('VectorUInt8', Uint8Array)
  Module.VectorInt32 = makeVectorType('VectorInt32', Int32Array)
  Module.VectorUInt32 = makeVectorType('VectorUInt32', Uint32Array)

  Module.VerifyScript = function (extendedTX, utxoHeights, blockHeight, consensus, customFlags) {
    return verifyScriptMain(extendedTX, utxoHeights, blockHeight, consensus, customFlags)
  }

  Module.VerifyScriptArray = Module.VerifyScript

  Module.VerifyScriptArrayNetwork = function (
    extendedTX, utxoHeights, blockHeight, consensus, customFlags, network
  ) {
    validateNetwork(network)
    const result = runWithBuffers(
      [uint8(extendedTX), int32(utxoHeights), uint32(customFlags)],
      2,
      ([tx, heights, flags], output) => Module._bdk_verify_script(
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
        Module._bdk_verify_script_batch(
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
      ([tx, script], output) => Module._bdk_verify_spend(
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
        Module._bdk_verify_spend_batch(
          txs.ptr, txs.length, txEnds.ptr, inputs.ptr,
          scripts.ptr, scripts.length, scriptEnds.ptr,
          satoshis.ptr, heights.ptr, blocks.ptr, modes.ptr, hasFlags.ptr, flags.ptr,
          count, network, output
        )
      }
    )
  }
}
