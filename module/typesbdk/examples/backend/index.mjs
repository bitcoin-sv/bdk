import createBdkModule from '../../wasm/bdk-core.mjs';

const bdk = await createBdkModule();
console.log('✅ WASM module loaded');

const blockHeight = 12345;
const consensus = true;
// Create embind vectors properly
const extendedTX = new bdk.VectorUInt8();
[0x01, 0x02, 0x03].forEach(v => extendedTX.push_back(v));

const utxoHeights = new bdk.VectorInt32();
[100, 200, 300].forEach(v => utxoHeights.push_back(v));

const customFlags = new bdk.VectorUInt32();
[1, 2].forEach(v => customFlags.push_back(v));

const result = bdk.VerifyScript(extendedTX,utxoHeights,blockHeight,consensus,customFlags);

console.log('VerifyScript returned:', result);
