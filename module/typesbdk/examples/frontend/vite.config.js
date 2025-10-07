import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

export default defineConfig({
  root: path.resolve(__dirname), // <- set project root to frontend folder
  plugins: [react()],
  resolve: {
    alias: {
      "@wasm": path.resolve(__dirname, "../../wasm"),
    },
  },
  server: {
    fs: {
      allow: [
        path.resolve(__dirname),            // frontend folder
        path.resolve(__dirname, "../../wasm"), // wasm folder
      ],
    },
  },
  build: {
    outDir: "dist", // optional: output folder for production build
  },
});
