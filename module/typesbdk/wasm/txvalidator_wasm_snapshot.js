/* global Module, HEAPU8 */

// Worker-capable builds can clone the expensive runtime verification tables.
// The workerless slim UMD build omits this internal scheduling-only ABI.
{
  function snapshotBytes (values) {
    if (values instanceof Uint8Array) return values
    if (values === null || values === undefined || typeof values.length !== 'number') {
      throw new TypeError('verification table snapshot must be an array or typed array')
    }
    return Uint8Array.from(values)
  }

  Module.ExportVerificationTables = function () {
    const size = Module._bdk_verification_table_snapshot_size()
    if (size === 0) throw new Error('invalid verification table snapshot size')
    const output = Module._malloc(size)
    if (output === 0) throw new Error(`unable to allocate ${size} verifier bytes`)
    try {
      if (Module._bdk_export_verification_tables(output, size) !== 1) {
        throw new Error('unable to export verification tables')
      }
      return Uint8Array.from(HEAPU8.subarray(output, output + size))
    } finally {
      Module._free(output)
    }
  }

  Module.ImportVerificationTables = function (snapshotValue) {
    const snapshot = snapshotBytes(snapshotValue)
    const input = Module._malloc(snapshot.length)
    if (input === 0) {
      throw new Error(`unable to allocate ${snapshot.length} verifier bytes`)
    }
    try {
      HEAPU8.set(snapshot, input)
      if (Module._bdk_import_verification_tables(input, snapshot.length) !== 1) {
        throw new Error('invalid verification table snapshot')
      }
    } finally {
      Module._free(input)
    }
  }
}
