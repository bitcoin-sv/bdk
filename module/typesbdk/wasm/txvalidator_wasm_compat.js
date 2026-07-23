/* global Module, HEAPU8, HEAP32, HEAPU32 */

// Legacy embind-style vectors are kept as a separate adapter. Modern consumers
// use the packed typed-array ABI and can omit this file without changing the
// verifier, cryptography, network, or batch functionality.
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
      return checkedHandle(this).store === checkedHandle(other).store
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
        if (!Number.isSafeInteger(size) || size < 0) {
          throw new RangeError('invalid vector size')
        }
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
      if (pointer === 0) {
        throw new Error(`unable to allocate ${byteLength} verifier bytes`)
      }
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

  function reusableResultPointer () {
    if (singleResultPointer === 0) {
      singleResultPointer = Module._malloc(2 * Int32Array.BYTES_PER_ELEMENT)
      if (singleResultPointer === 0) {
        throw new Error('unable to allocate verifier result')
      }
    }
    return singleResultPointer
  }

  const typedVerifyScript = Module.VerifyScript
  Module.VectorUInt8 = makeVectorType('VectorUInt8', Uint8Array)
  Module.VectorInt32 = makeVectorType('VectorInt32', Int32Array)
  Module.VectorUInt32 = makeVectorType('VectorUInt32', Uint32Array)
  Module.VerifyScript = function (
    extendedTX,
    utxoHeights,
    blockHeight,
    consensus,
    customFlags
  ) {
    const txHandle = extendedTX?.[vectorHandle]
    const heightHandle = utxoHeights?.[vectorHandle]
    const flagHandle = customFlags?.[vectorHandle]
    if (
      txHandle === undefined ||
      heightHandle === undefined ||
      flagHandle === undefined
    ) {
      return typedVerifyScript(
        extendedTX, utxoHeights, blockHeight, consensus, customFlags
      )
    }
    const tx = checkedHandle(extendedTX).store
    const heights = checkedHandle(utxoHeights).store
    const flags = checkedHandle(customFlags).store
    if (
      tx.Type !== Uint8Array ||
      heights.Type !== Int32Array ||
      flags.Type !== Uint32Array
    ) {
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
    return {
      domain: HEAP32[output >> 2],
      code: HEAP32[(output >> 2) + 1]
    }
  }
}
