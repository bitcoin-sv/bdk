import assert from 'node:assert/strict'
import { webcrypto } from 'node:crypto'
import { readFile } from 'node:fs/promises'
import vm from 'node:vm'

import { runBdkTestSuite } from './test-suite.mjs'

const moduleName = process.argv[2] ?? '../../module/typesbdk/wasm/bdk-core.umd.js'
const moduleUrl = new URL(moduleName, import.meta.url)
const wasmUrl = new URL(moduleName.replace(/\.js$/, '.wasm'), import.meta.url)
const script = await readFile(moduleUrl, 'utf8')
const wasmBinary = await readFile(wasmUrl)
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
  crypto: webcrypto,
  performance,
  window: undefined
})
context.window = context
vm.runInContext(script, context, { filename: moduleName })
assert.equal(typeof context.createBdkModule, 'function')
const bdk = await context.createBdkModule({ wasmBinary })
assert.equal(typeof bdk.VerifyScriptBatchArray, 'function')
assert.equal(typeof bdk.VerifySpendBatchArray, 'function')
assert.equal(typeof bdk.SignDigest, 'function')
assert.equal(typeof bdk.VerifyDigestBatchArray, 'function')
assert.equal(typeof bdk.MultiplyPublicKey, 'function')
bdk.PrepareVerification()
bdk.PrepareSigning()
if (!moduleName.includes('.slim.')) {
  assert.equal(typeof bdk.ExportVerificationTables, 'function')
  assert.equal(typeof bdk.ImportVerificationTables, 'function')
  assert.equal(typeof bdk.VectorUInt8, 'function')
  assert.equal(typeof bdk.VerifyScript, 'function')
}
await runBdkTestSuite({
  bdk,
  createBdkModule: context.createBdkModule,
  moduleOptions: { wasmBinary },
  label: moduleName
})
