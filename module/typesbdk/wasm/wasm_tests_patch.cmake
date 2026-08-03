# Generate a build-tree patched copy of libsecp256k1's tests.c for the WASM
# runtime-table parity suite. See wasm_tests.c's header for why this is needed --
# the runtime lambda-table substitution and the two breakage modes it works around.
#
# Three anchored edits are applied; every other line is byte-identical to upstream:
#   1. run_ecmult_pre_g -- replace the 2^128*G first-entry check with the lambda
#      expectation, since the substituted pre_g_128 starts from lambda*G;
#   2. test_fixed_wnaf -- route its split_128 call through the alias so it keeps
#      exercising the plain 128-bit split under the wrapper's macro; and
#   3. main() -- inject bdk_secp256k1_prepare_verification_tables() as the first
#      statement so every test runs against the populated runtime tables.
# The edits are fail-loud: if an anchor matches anything other than exactly once,
# this script aborts, so an upstream refactor can never silently drop an adaptation.
#
# Invoked as:
#   cmake -Din=<path/to/tests.c> -Dout=<path/to/patched/tests.c> -P wasm_tests_patch.cmake

if(NOT DEFINED in OR NOT DEFINED out)
  message(FATAL_ERROR "wasm_tests_patch.cmake: -Din=<tests.c> and -Dout=<file> are required")
endif()
if(NOT EXISTS "${in}")
  message(FATAL_ERROR "wasm_tests_patch.cmake: input file does not exist: ${in}")
endif()

file(READ "${in}" _src)

# Replace an anchor exactly once, aborting if it occurs zero or many times.
function(bdk_replace_once var anchor replacement)
  set(_text "${${var}}")
  string(REPLACE "${anchor}" "" _stripped "${_text}")
  string(LENGTH "${_text}" _len_full)
  string(LENGTH "${_stripped}" _len_stripped)
  string(LENGTH "${anchor}" _len_anchor)
  if(_len_anchor EQUAL 0)
    message(FATAL_ERROR "wasm_tests_patch.cmake: empty anchor")
  endif()
  math(EXPR _count "(${_len_full} - ${_len_stripped}) / ${_len_anchor}")
  if(NOT _count EQUAL 1)
    message(FATAL_ERROR
      "wasm_tests_patch.cmake: anchor matched ${_count} time(s), expected exactly 1.\n"
      "Anchor:\n${anchor}")
  endif()
  string(REPLACE "${anchor}" "${replacement}" _text "${_text}")
  set(${var} "${_text}" PARENT_SCOPE)
endfunction()

# Edit 1 (run_ecmult_pre_g): the substituted secp256k1_pre_g_128 holds odd
# multiples of lambda*G, not of 2^128*G, so its first entry is lambda*G = (beta*Gx, Gy).
# Replace the 2^128*G equality check with the lambda expectation.
set(_anchor_geometry [==[CHECK(secp256k1_memcmp_var(&gs, &secp256k1_pre_g_128[0], sizeof(gs)) == 0);]==])
set(_replace_geometry [==[do { secp256k1_ge bdk_lambda_g = secp256k1_ge_const_g; secp256k1_ge bdk_expected_g; secp256k1_ge_mul_lambda(&bdk_lambda_g, &bdk_lambda_g); secp256k1_ge_from_storage(&bdk_expected_g, &secp256k1_pre_g_128[0]); CHECK(secp256k1_fe_equal(&bdk_lambda_g.x, &bdk_expected_g.x)); CHECK(secp256k1_fe_equal(&bdk_lambda_g.y, &bdk_expected_g.y)); } while(0);]==])

# Edit 2 (test_fixed_wnaf): the wrapper redefines secp256k1_scalar_split_128 to
# the lambda split for the ecmult generator lane, but this helper genuinely needs
# the plain 128-bit split. Route it through the alias the wrapper captured before
# the redefinition.
set(_anchor_split [==[secp256k1_scalar_split_128(&num, &unused, number);]==])
set(_replace_split [==[bdk_wasm_split_128(&num, &unused, number);]==])

# Edit 3 (main): prime the runtime verification tables as the first statement of
# main(), so every test runs against the populated tables. An explicit call is used
# rather than a constructor so the priming cannot be reordered or elided by the
# build's link-time optimisation.
set(_anchor_main [==[int main(int argc, char **argv) {]==])
set(_replace_main [==[int main(int argc, char **argv) {
    bdk_secp256k1_prepare_verification_tables();]==])

bdk_replace_once(_src "${_anchor_geometry}" "${_replace_geometry}")
bdk_replace_once(_src "${_anchor_split}" "${_replace_split}")
bdk_replace_once(_src "${_anchor_main}" "${_replace_main}")

file(WRITE "${out}" "${_src}")
message(STATUS "wasm_tests_patch.cmake: wrote patched tests.c to ${out}")
