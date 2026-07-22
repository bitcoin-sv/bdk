import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import vm from 'node:vm'

const script = await readFile(new URL('./bdk-core.umd.js', import.meta.url), 'utf8')
const wasmBinary = await readFile(new URL('./bdk-core.umd.wasm', import.meta.url))
const context = vm.createContext({
  console,
  TextDecoder,
  TextEncoder,
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  ArrayBuffer,
  WebAssembly,
  performance,
  window: undefined
})
context.window = context
vm.runInContext(script, context, { filename: 'bdk-core.umd.js' })
assert.equal(typeof context.createBdkModule, 'function')
const bdk = await context.createBdkModule({ wasmBinary })
assert.equal(typeof bdk.VerifyScriptBatchArray, 'function')
assert.equal(typeof bdk.VerifySpendBatchArray, 'function')
console.log('ok - UMD browser loader exposes transaction and Spend batch ABIs')
