// Cross-check the shipped module's ECDSA verification against the shared corpus
// produced by verify-corpus-gen.mjs. Every valid signature must verify and every
// tampered one must be rejected; because verification runs through the substituted
// runtime tables (secp256k1_pre_g / secp256k1_pre_g_128), a wrong table entry
// surfaces here as a valid signature failing to verify. This is whole-pipeline
// agreement over the corpus inputs, not a proof of wNAF lane coverage (that comes
// from the wasm_secp256k1_tests / _noverify_tests unit suites).

import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'

const moduleName = process.argv[2] ?? 'bdk-core.mjs'
const corpusPath = process.argv[3]
if (!corpusPath) throw new Error('usage: test-verify-parity.mjs <module> <corpus.json>')

const isBrowserBuild = moduleName.includes('.browser.')
const moduleUrl = new URL(moduleName, import.meta.url)
if (isBrowserBuild) globalThis.window = globalThis
const { default: createBdkModule } = await import(moduleUrl.href)
const moduleOptions = isBrowserBuild
  ? { wasmBinary: await readFile(new URL(moduleName.replace(/\.mjs$/, '.wasm'), import.meta.url)) }
  : {}
const bdk = await createBdkModule(moduleOptions)

function fromHex (hex) {
  const bytes = new Uint8Array(hex.length / 2)
  for (let i = 0; i < bytes.length; i++) bytes[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16)
  return bytes
}

const vectors = JSON.parse(await readFile(corpusPath, 'utf8'))
assert.ok(vectors.length > 0, 'verification corpus must not be empty')

let verified = 0
for (const vector of vectors) {
  const actual = bdk.VerifyDigest(
    fromHex(vector.publicKey),
    fromHex(vector.digest),
    fromHex(vector.signature)
  )
  assert.equal(actual, vector.expected,
    `${vector.name}: module verdict ${actual} does not match expected ${vector.expected}`)
  verified++
}

console.log(`ok - ${verified} verification vectors match across the substituted tables`)
