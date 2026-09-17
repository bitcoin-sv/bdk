#include <cstdint>
#include <cstdlib>

/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_gobdk_shim
#else
#define BOOST_TEST_MODULE test_gobdk_shimd
#endif

#include <boost/test/unit_test.hpp>

#include <bdkcgo/abierror_cgo.h>
#include <bdkcgo/txvalidator_cgo.h>
#include <bdkcgo/validatebatch_cgo.h>
#include "txerror.h"

// These cases exercise the CGO shim directly, without going through cgo. They assert
// the boundary contract the Go wrapper relies on: a buffer argument that cannot be
// expressed is reported as a typed ABI result, and no memory is read to produce it.

BOOST_AUTO_TEST_SUITE(test_gobdk_shim_suite)

namespace {

const char kNetwork[] = "main";
const char kDummyTx[] = { 0x00 };
const int32_t kHeights[] = { 0 };
const uint32_t kFlags[] = { 0 };

bool IsAbiError(TxError result, AbiError_t code)
{
    return (result.domain == static_cast<int32_t>(TX_ERR_DOMAIN_ABI))
        && (result.code == static_cast<int32_t>(code));
}

struct ValidatorFixture {
    TxValidatorCGO engine{ nullptr };

    ValidatorFixture() { engine = TxValidator_CreateV2(kNetwork, sizeof(kNetwork) - 1); }
    ~ValidatorFixture() { TxValidator_Destroy(engine); }
};

struct BatchFixture {
    ValidateBatchCGO batch{ nullptr };

    BatchFixture() { batch = ValidateBatch_CreateV2(); }
    ~BatchFixture() { ValidateBatch_Destroy(batch); }
};

} // namespace

// T-S1a — the entry points that return a TxError report a null buffer with a positive
// length as { TX_ERR_DOMAIN_ABI, ABI_ERR_NULL_BUFFER }. Nothing is dereferenced to get
// there, which is the property under test.
BOOST_FIXTURE_TEST_CASE(null_buffer_is_a_typed_abi_result, ValidatorFixture)
{
    BOOST_REQUIRE(engine != nullptr);

    // Extended transaction pointer
    BOOST_CHECK(IsAbiError(
        TxValidator_ValidateTransaction(engine, nullptr, 16, kHeights, 1, 0, true),
        ABI_ERR_NULL_BUFFER));
    BOOST_CHECK(IsAbiError(
        TxValidator_VerifyScript(engine, nullptr, 16, kHeights, 1, 0, true),
        ABI_ERR_NULL_BUFFER));
    BOOST_CHECK(IsAbiError(
        TxValidator_VerifyScriptWithCustomFlags(engine, nullptr, 16, kHeights, 1, 0, true, kFlags, 1),
        ABI_ERR_NULL_BUFFER));

    // UTXO heights pointer
    BOOST_CHECK(IsAbiError(
        TxValidator_ValidateTransaction(engine, kDummyTx, 1, nullptr, 1, 0, true),
        ABI_ERR_NULL_BUFFER));
    BOOST_CHECK(IsAbiError(
        TxValidator_VerifyScript(engine, kDummyTx, 1, nullptr, 1, 0, true),
        ABI_ERR_NULL_BUFFER));
    BOOST_CHECK(IsAbiError(
        TxValidator_VerifyScriptWithCustomFlags(engine, kDummyTx, 1, nullptr, 1, 0, true, kFlags, 1),
        ABI_ERR_NULL_BUFFER));

    // Custom flags pointer
    BOOST_CHECK(IsAbiError(
        TxValidator_VerifyScriptWithCustomFlags(engine, kDummyTx, 1, kHeights, 1, 0, true, nullptr, 1),
        ABI_ERR_NULL_BUFFER));
}

// T-S1b — TxValidator_GetSigOpCount has no TxError channel: it returns a count and
// reports through errStr, which the caller frees. strdup is malloc-family, so free is
// the correct deallocator.
BOOST_FIXTURE_TEST_CASE(sigopcount_reports_abi_failure_through_errstr, ValidatorFixture)
{
    BOOST_REQUIRE(engine != nullptr);

    char* errStr = nullptr;
    const uint64_t count = TxValidator_GetSigOpCount(engine, nullptr, 16, kHeights, 1, 0, true, true, &errStr);

    BOOST_CHECK_EQUAL(count, uint64_t{ 0 });
    BOOST_REQUIRE(errStr != nullptr);
    free(errStr);
}

// T-S2 — the batch contract: Add reports whether the entry was appended, a rejected
// entry leaves the size unchanged, and an unsatisfiable Reserve returns rather than
// letting an exception cross extern "C".
BOOST_FIXTURE_TEST_CASE(batch_add_and_reserve_contract, BatchFixture)
{
    BOOST_REQUIRE(batch != nullptr);
    BOOST_CHECK_EQUAL(ValidateBatch_Size(batch), uint64_t{ 0 });

    const TxError added = ValidateBatch_Add(batch, kDummyTx, 1, kHeights, 1, 0, true);
    BOOST_CHECK_EQUAL(added.domain, static_cast<int32_t>(TX_ERR_DOMAIN_OK));
    BOOST_CHECK_EQUAL(ValidateBatch_Size(batch), uint64_t{ 1 });

    const TxError rejected = ValidateBatch_Add(batch, nullptr, 16, kHeights, 1, 0, true);
    BOOST_CHECK(IsAbiError(rejected, ABI_ERR_NULL_BUFFER));
    BOOST_CHECK_EQUAL(ValidateBatch_Size(batch), uint64_t{ 1 });

    // Best effort, and never a throw across the boundary.
    BOOST_CHECK_NO_THROW(ValidateBatch_Reserve(batch, UINT64_MAX));
    BOOST_CHECK_EQUAL(ValidateBatch_Size(batch), uint64_t{ 1 });

    ValidateBatch_Clear(batch);
    BOOST_CHECK_EQUAL(ValidateBatch_Size(batch), uint64_t{ 0 });
    BOOST_CHECK(ValidateBatch_Empty(batch));
}

// A zero length is not an ABI failure: it is an empty buffer, and stays a parse
// failure in the exception domain exactly as before.
BOOST_FIXTURE_TEST_CASE(empty_buffer_is_not_an_abi_failure, ValidatorFixture)
{
    BOOST_REQUIRE(engine != nullptr);

    const TxError result = TxValidator_ValidateTransaction(engine, nullptr, 0, nullptr, 0, 0, true);
    BOOST_CHECK_EQUAL(result.domain, static_cast<int32_t>(TX_ERR_DOMAIN_EXCEPTION));
}

// The echo entry point is the generation marker the Go suite uses; assert here too
// that the boundary carries a full 64-bit length.
BOOST_AUTO_TEST_CASE(echo_length_crosses_unchanged)
{
    BOOST_CHECK_EQUAL(TxValidator_ABI_EchoLength(0), uint64_t{ 0 });
    BOOST_CHECK_EQUAL(TxValidator_ABI_EchoLength(UINT64_MAX), UINT64_MAX);
    BOOST_CHECK_EQUAL(TxValidator_ABI_EchoLength(uint64_t{ 2160000188 }), uint64_t{ 2160000188 });
}

BOOST_AUTO_TEST_SUITE_END()
