/* MINIMAL_RUNTIME expects bytes rather than loading its companion automatically. */
if (Module.wasm === undefined) {
  if (Module.wasmBinary !== undefined) {
    Module.wasm = Module.wasmBinary
  } else {
    const prefix = bdkWasmScriptUrl === undefined
      ? ''
      : new URL('.', bdkWasmScriptUrl).href
    const path = Module.locateFile?.('bdk-core.umd.wasm', prefix) ??
      `${prefix}bdk-core.umd.wasm`
    const response = await fetch(path, { credentials: 'same-origin' })
    if (!response.ok) throw new Error(`${response.status} : ${response.url}`)
    Module.wasm = await response.arrayBuffer()
  }
}
