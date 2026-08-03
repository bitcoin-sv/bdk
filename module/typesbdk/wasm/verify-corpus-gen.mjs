// Generate the shared ECDSA verification corpus (fixture wasm_secp256k1_verify_corpus).
// This is a WHOLE-PIPELINE agreement check over a broad spread of edge and
// deterministic-random inputs: for
// each (private key, digest) it produces a valid RFC 6979 signature (expected to
// verify) plus tampered variants (expected to be rejected). Signing uses the static
// signing comb table, which is NOT substituted, so a valid signature that fails to
// verify would expose a wrong verification-table entry through the substituted path.
//
// Scope: this leg proves end-to-end sign/verify agreement over many inputs; it does
// NOT prove that both post-verification fixed-base wNAF lanes and a wide window range
// are hit, because verification derives u1/u2 from (r, s, z) rather than from the
// chosen inputs. That table-level coverage is provided by the upstream unit suites
// (run_ecmult_* in wasm_secp256k1_tests / _noverify_tests), which exercise the tables
// exhaustively at unit depth.
//
// Expected verdicts are definitional (a correct signature over a digest by a key
// verifies under that key's public key; a corrupted signature or digest does not),
// so no separate oracle build is needed. The corpus is written to the build tree
// and consumed by test-verify-parity.mjs.

import { writeFile } from 'node:fs/promises'
import { createHash } from 'node:crypto'

const moduleName = process.argv[2] ?? 'bdk-core.mjs'
const outputPath = process.argv[3]
if (!outputPath) throw new Error('usage: verify-corpus-gen.mjs <module> <output-corpus.json>')

const isBrowserBuild = moduleName.includes('.browser.')
const moduleUrl = new URL(moduleName, import.meta.url)
if (isBrowserBuild) globalThis.window = globalThis
const { default: createBdkModule } = await import(moduleUrl.href)
const moduleOptions = isBrowserBuild
  ? { wasmBinary: await (await import('node:fs/promises')).readFile(new URL(moduleName.replace(/\.mjs$/, '.wasm'), import.meta.url)) }
  : {}
const bdk = await createBdkModule(moduleOptions)

function fromHex (hex) {
  const bytes = new Uint8Array(hex.length / 2)
  for (let i = 0; i < bytes.length; i++) bytes[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16)
  return bytes
}
function toHex (bytes) {
  return Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('')
}

// secp256k1 group order n; every private key below is a non-zero value < n.
const privateKeys = [
  '0000000000000000000000000000000000000000000000000000000000000001',
  '0000000000000000000000000000000000000000000000000000000000000002',
  'fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140', // n - 1
  'fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364130', // n - 17
  '7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a0', // approx n/2
  '00000000000000000000000000000001000000000000000000000000000000ff', // spans the 128-bit halves
  '00000000000000000000000000000000ffffffffffffffffffffffffffffffff', // low 128 bits set
  'fffffffffffffffffffffffffffffffdffffffffffffffffffffffffffffffff', // high 128 bits set, < n
  '123456789abcdef0fedcba98765432100123456789abcdef0fedcba987654321',
  'a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a55a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5'
]

const digests = [
  '0000000000000000000000000000000000000000000000000000000000000000',
  '0000000000000000000000000000000000000000000000000000000000000001',
  'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff',
  '4f3c2f0f55e2f4f08c892a2ca2c10100c33c5e7f443f3d13f86af68b4f8f25de',
  '8000000000000000000000000000000000000000000000000000000000000000'
]

const vectors = []

// Emit a valid vector plus a signature-tampered and a digest-tampered variant.
function addVectors (label, privateKey, digestBytes) {
  const publicKey = bdk.PublicKeyFromPrivate(privateKey)
  const digest = digestBytes
  const signature = bdk.SignDigest(privateKey, digest)

  vectors.push({
    name: `${label}/valid`,
    publicKey: toHex(publicKey),
    digest: toHex(digest),
    signature: toHex(signature),
    expected: true
  })

  const flippedSignature = signature.slice()
  flippedSignature[flippedSignature.length - 1] ^= 0x01
  vectors.push({
    name: `${label}/sig-tampered`,
    publicKey: toHex(publicKey),
    digest: toHex(digest),
    signature: toHex(flippedSignature),
    expected: false
  })

  const flippedDigest = digest.slice()
  flippedDigest[0] ^= 0x80
  vectors.push({
    name: `${label}/digest-tampered`,
    publicKey: toHex(publicKey),
    digest: toHex(flippedDigest),
    signature: toHex(signature),
    expected: false
  })
}

// Edge-scalar section: hand-picked private keys and digests.
for (let keyIndex = 0; keyIndex < privateKeys.length; keyIndex++) {
  const privateKey = fromHex(privateKeys[keyIndex])
  // Pair each key with two digests so both halves of the corpus stay small but broad.
  for (const digestHex of [digests[keyIndex % digests.length], digests[(keyIndex + 2) % digests.length]]) {
    addVectors(`edge-key${keyIndex}/${digestHex.slice(0, 8)}`, privateKey, fromHex(digestHex))
  }
}

// Deterministic pseudo-random expansion: a fixed-seed SHA-256 counter stream (no
// external entropy, so the corpus is reproducible). This broadens the spread of
// (private key, digest) inputs. It does NOT by itself prove post-verification wNAF
// lane/window coverage -- verification derives u1/u2 from (r, s, z), which are not
// chosen here; that coverage comes from the upstream unit suites (run_ecmult_*).
function seededBytes (seed, index, length) {
  let out = Buffer.alloc(0)
  let counter = 0
  while (out.length < length) {
    out = Buffer.concat([out, createHash('sha256').update(`${seed}:${index}:${counter}`).digest()])
    counter++
  }
  return new Uint8Array(out.subarray(0, length))
}

const RANDOM_PAIRS = 32
for (let i = 0; i < RANDOM_PAIRS; i++) {
  const privateKey = seededBytes('bdk-parity-key', i, 32)
  // Force the most-significant byte below the group order's (0xff) and avoid zero,
  // so every generated key is a valid scalar in [1, n) without rejection sampling.
  privateKey[0] &= 0x7f
  if (privateKey.every(b => b === 0)) privateKey[31] = 1
  const digest = seededBytes('bdk-parity-digest', i, 32)
  addVectors(`rand${i}`, privateKey, digest)
}

await writeFile(outputPath, JSON.stringify(vectors, null, 2))
console.log(`ok - wrote ${vectors.length} verification vectors to ${outputPath}`)
