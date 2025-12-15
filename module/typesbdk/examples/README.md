## To build wasm

The Wasm module is built using core-only components. This ensures the engine includes a minimal set of files, effectively avoiding dependency explosion issues.


```
set -e ; export EM_CONFIG=/path/to/emsdk/installation/.emscripten ; source /path/to/emsdk/installation/emsdk_env.sh ; export CROSS_COMPILE="" ; export CXX=em++ ; export CC=emcc ; export AR=emar ; export RANLIB=emranlib ; export PAGER=cat ; export LESS=F ; export MORE=cat ; export MANPAGER=cat ; export EMCC_FORCE_STDIN_INPUT=0
rm -fR /path/to/build/directory/build/* && cd /path/to/build/directory/build
cmake ../bdk/ -DCMAKE_BUILD_TYPE=Release -DBDK_BUILD_CORE_ONLY=ON -DBOOST_ROOT=/path/to/dependancies_wasm/boost-1.85 -DOPENSSL_ROOT_DIR=/path/to/dependancies_wasm/openssl_3.4.0 && make -j6
```

After building, the provided examples may be executed to test both backend and frontend environments. Please note that these tests are designed solely to confirm that the Wasm module loads correctly and that VerifyScript is callable. Validation of the function's logic using real data is out of scope for this iteration and will be addressed in the next phase.

### To run example backend

```
rm -rf /path/to/bdk/module/typesbdk/examples/backend/node_modules && cd /path/to/bdk/module/typesbdk/examples/backend
node /path/to/bdk/module/typesbdk/examples/backend/index.mjs
```

### To run example frontend

```
rm -rf /path/to/bdk/module/typesbdk/examples/frontend/node_modules && cd /path/to/bdk/module/typesbdk/examples/frontend
yarn install && yarn run start
# Then open the browser and click on "verify"
```