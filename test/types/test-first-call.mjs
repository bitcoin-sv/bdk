// First-call parity for TweakPublicKeyAdd. TweakPublicKeyAdd reaches the fixed-base
// tables through secp256k1_ec_pubkey_tweak_add but does not itself call the prepare
// routine (unlike the verify entry points in txvalidator_wasm.cpp), so
// on a freshly instantiated module -- before any verify or explicit
// PrepareVerification -- it operates on the all-zero runtime tables and returns a
// wrong point.
//
// This is a known-bug reproducer with inverted exit semantics, so it does NOT need
// (and must not use) the CTest WILL_FAIL property, which would mask real setup or
// prepared-path regressions:
//
//   * exit 0  -- and only 0 -- when every invariant holds (the module loads, the
//                prepared path succeeds and matches the known answer) AND the known
//                first-call bug reproduces on every case (first-call != prepared).
//                CI stays green in this expected state.
//   * exit 1  when first-call parity holds (the defensive prepare fix has landed):
//                the entry goes red so it is flipped to a must-pass parity test.
//   * exit 1  for any real regression -- module load failure, a prepared tweak
//                failing, or a prepared result not matching the known answer -- via
//                the assertions below, which throw.
//
// A fresh module instance is created for every case; wasm instances have isolated
// memory, so the unprepared instance genuinely runs on zero tables.

import assert from 'node:assert/strict'

const moduleName = process.argv[2] ?? '../../module/typesbdk/wasm/bdk-core.mjs'
const isBrowserBuild = moduleName.includes('.browser.')
const moduleUrl = new URL(moduleName, import.meta.url)

if (isBrowserBuild) globalThis.window = globalThis
const { default: createBdkModule } = await import(moduleUrl.href)
const moduleOptions = isBrowserBuild
  ? { wasmBinary: await (await import('node:fs/promises')).readFile(new URL(moduleName.replace(/\.mjs$/, '.wasm'), import.meta.url)) }
  : {}

function fromHex (hex) {
  const bytes = new Uint8Array(hex.length / 2)
  for (let i = 0; i < bytes.length; i++) bytes[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16)
  return bytes
}
function toHex (bytes) {
  return Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('')
}

const G = fromHex('0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798')
const twoG = fromHex('02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5')
const tweak = value => fromHex(value.toString(16).padStart(64, '0'))

const cases = [
  { name: 'G + 1 == 2G', publicKey: G, tweak: tweak(1), known: twoG },
  { name: 'G + 2', publicKey: G, tweak: tweak(2) },
  { name: '2G + 1', publicKey: twoG, tweak: tweak(1) },
  { name: 'G + 0x2a', publicKey: G, tweak: tweak(0x2a) }
]

// Attempt the tweak, capturing a thrown "unable to tweak" as a distinct value so
// a wrong-point result and a rejection both compare unequal to the correct answer.
function tweakResult (bdk, publicKey, tweakBytes) {
  try {
    return { ok: true, hex: toHex(bdk.TweakPublicKeyAdd(publicKey, tweakBytes)) }
  } catch (error) {
    return { ok: false, hex: `THREW:${error.message}` }
  }
}

let bugReproduced = 0
let parityHeld = 0
for (const testCase of cases) {
  // Fresh instance, tweak as the very first curve operation (no prepare).
  const firstInstance = await createBdkModule(moduleOptions)
  const firstCall = tweakResult(firstInstance, testCase.publicKey, testCase.tweak)

  // Fresh instance, prepared first: this is the correct reference result.
  const preparedInstance = await createBdkModule(moduleOptions)
  preparedInstance.PrepareVerification()
  const prepared = tweakResult(preparedInstance, testCase.publicKey, testCase.tweak)

  // Invariants -- a failure here is a real regression, not the known bug; assert
  // throws, node exits non-zero.
  assert.equal(prepared.ok, true, `${testCase.name}: prepared tweak must succeed`)
  if (testCase.known) {
    assert.equal(prepared.hex, toHex(testCase.known),
      `${testCase.name}: prepared path must match the known answer`)
  }

  if (firstCall.ok && firstCall.hex === prepared.hex) {
    parityHeld++
    console.log(`parity - ${testCase.name}: first-call=${firstCall.hex} prepared=${prepared.hex}`)
  } else {
    bugReproduced++
    console.log(`MISMATCH (bug reproduced) - ${testCase.name}: first-call=${firstCall.hex} prepared=${prepared.hex}`)
  }
}

if (parityHeld === 0) {
  console.log(
    `ok - known first-call bug reproduced on all ${cases.length} cases; ` +
    'this entry stays green until the defensive prepare fix lands.')
  process.exit(0)
}

console.error(
  `FIRST-CALL PARITY NOW HOLDS for ${parityHeld}/${cases.length} case(s): the fix has ` +
  'landed. Make wasm_secp256k1_first_call_parity a must-pass parity test ' +
  '(assert first-call === prepared) and remove this known-failing inversion.')
process.exit(1)
