// Minimal Node example against the committed WASM module.
//
// It uses the typed-array API the module actually ships: `VerifyScriptArray` (also
// exposed as `VerifyScript`) takes plain `Uint8Array`/`Int32Array`/`Uint32Array`
// values and copies them into WASM memory itself. There is nothing to allocate and
// nothing to free. The embind-style `VectorUInt8`/`VectorInt32`/`VectorUInt32` shims
// exist only in the compatibility layer, which the slim UMD build does not include,
// so new code should not use them.
//
// The transaction below is in Extended Format (EF): each input carries its source
// satoshi amount and locking script inline, which is what lets the verifier run
// without any UTXO lookup. `utxoHeights` holds one entry per input.

import createBdkModule from '../../wasm/bdk-core.mjs';

const bdk = await createBdkModule();
console.log('✅ WASM module loaded');

// A real mainnet P2PKH spend (block 620940), the same vector the test suite uses.
const extendedTxHex =
  '010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d70' +
  '00000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda02' +
  '20717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec' +
  '3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14' +
  'a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff024' +
  '6afa795658812b42788ac00000000';

function fromHex (hex) {
  const bytes = new Uint8Array(hex.length / 2);
  for (let index = 0; index < bytes.length; index++) {
    bytes[index] = Number.parseInt(hex.slice(index * 2, index * 2 + 2), 16);
  }
  return bytes;
}

const extendedTX = fromHex(extendedTxHex);
const utxoHeights = Int32Array.from([574441]); // one per input
const blockHeight = 620940;
const consensus = true;
const customFlags = new Uint32Array(); // none: use the network defaults

const result = bdk.VerifyScriptArray(
  extendedTX,
  utxoHeights,
  blockHeight,
  consensus,
  customFlags
);

// Structured verdict: domain 0 is success, 1 a script failure, 2 a
// transaction-validation/DoS-class failure, 3 a caught exception. Never treat an
// unrecognized domain as success.
console.log('VerifyScriptArray returned:', result);
if (result.domain !== 0) {
  console.error(`verification failed: domain=${result.domain} code=${result.code}`);
  process.exit(1);
}
console.log('✅ transaction verified');
