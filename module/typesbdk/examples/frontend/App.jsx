import React, { useState, useEffect } from "react";
//import createBdkModule from "../../wasm/bdk.mjs";
import createBdkModule from "@wasm/bdk-core.mjs";

export default function App() {
  const [bdk, setBdk] = useState(null);
  const [message, setMessage] = useState("");

  // Initialize WASM module
  useEffect(() => {
    async function init() {
      const module = await createBdkModule();
      setBdk(module);
    }
    init();
  }, []);

  const handleVerify = async () => {
    if (!bdk) {
      setMessage("WASM module not loaded yet.");
      return;
    }

    try {
      // Typed arrays
      const extendedTX = new Uint8Array([1, 2, 3]);
      const utxoHeights = new Int32Array([100, 200, 300]);
      const customFlags = new Uint32Array([1, 2]);

      // Wrap them using Module.Vector types if needed
      const extTXVec = new bdk.VectorUInt8();
      for (let i = 0; i < extendedTX.length; i++) extTXVec.push_back(extendedTX[i]);

      const utxoVec = new bdk.VectorInt32();
      for (let i = 0; i < utxoHeights.length; i++) utxoVec.push_back(utxoHeights[i]);

      const flagsVec = new bdk.VectorUInt32();
      for (let i = 0; i < customFlags.length; i++) flagsVec.push_back(customFlags[i]);

      // Call the function
      const result = bdk.VerifyScript(extTXVec, utxoVec, 12345, true, flagsVec);

      // Clean up memory
      extTXVec.delete();
      utxoVec.delete();
      flagsVec.delete();

      setMessage("Verify Result: " + result);
    } catch (err) {
      setMessage("Error: " + err);
    }
  };

  const handleClear = () => {
    setMessage("");
  };

  return (
    <div style={{ padding: "20px" }}>
      <button onClick={handleVerify}>Verify</button>
      <button onClick={handleClear} style={{ marginLeft: "10px" }}>
        Clear
      </button>
      <div style={{ marginTop: "20px" }}>{message}</div>
    </div>
  );
}
