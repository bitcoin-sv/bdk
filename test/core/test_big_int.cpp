// Why this test exists: bdk ships two implementations of the consensus-critical
// bsv::bint script arithmetic - bitcoin-sv's OpenSSL BIGNUM backend (compiled
// into the native core) and the wasm verifier's Boost multiprecision backend
// (module/typesbdk/wasm/big_int_boost.cpp). This one suite runs identically
// against both: test_big_int links the native core, test_big_int_boost compiles
// the Boost backend. Any behavioral divergence between the two - a silent
// wasm-vs-native consensus split - fails a test here instead of shipping.

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_big_int
#else
#define BOOST_TEST_MODULE test_big_intd
#endif

#include <boost/test/unit_test.hpp>

#include <big_int.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using bsv::bint;

BOOST_AUTO_TEST_SUITE(test_big_int)

BOOST_AUTO_TEST_CASE(construction_and_conversion)
{
    const std::array<int64_t, 9> values{
        std::numeric_limits<int64_t>::min(), -4294967296LL, -1, 0, 1,
        2147483648LL, 4294967296LL, 9223372036854775806LL,
        std::numeric_limits<int64_t>::max()
    };
    for(const int64_t value : values) {
        const bint number{value};
        BOOST_CHECK_EQUAL(bsv::to_dec(number), std::to_string(value));
        BOOST_CHECK_EQUAL(bsv::to_int64_t(number), value);
    }

    BOOST_CHECK_EQUAL(bsv::to_dec(bint{"123456789012345678901234567890"}),
                      "123456789012345678901234567890");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{"-12345678901234567890trailing"}),
                      "-12345678901234567890");
    BOOST_CHECK_THROW(bint{""}, bsv::big_int_error);
    BOOST_CHECK_THROW(bint{"not-a-number"}, bsv::big_int_error);
    BOOST_CHECK_THROW(
        bsv::to_int64_t(bint{"9223372036854775808"}), bsv::big_int_error);
    BOOST_CHECK_EQUAL(bsv::to_hex(bint{0}), "0");
    BOOST_CHECK_EQUAL(bsv::to_hex(bint{1}), "01");
    BOOST_CHECK_EQUAL(bsv::to_hex(bint{-255}), "-FF");
}

BOOST_AUTO_TEST_CASE(arithmetic_and_signed_remainders)
{
    const bint large{"123456789012345678901234567890"};
    const bint other{"98765432109876543210"};
    BOOST_CHECK_EQUAL(bsv::to_dec(large + other), "123456789111111111011111111100");
    BOOST_CHECK_EQUAL(bsv::to_dec(large - other), "123456788913580246791358024680");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{"123456789"} * bint{"987654321"}),
                      "121932631112635269");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{-7} / bint{3}), "-2");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{-7} % bint{3}), "-1");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{7} % bint{-3}), "1");
    BOOST_CHECK_THROW(bint{1} / bint{0}, bsv::big_int_error);
    BOOST_CHECK_THROW(bint{1} % bint{0}, bsv::big_int_error);
    BOOST_CHECK_EQUAL(bsv::to_dec(bsv::pow(bint{-2}, bint{5})), "-32");
    BOOST_CHECK_EQUAL(bsv::to_dec(bsv::pow(bint{99}, bint{0})), "1");
    BOOST_CHECK_EQUAL(bsv::to_dec(bsv::pow(bint{2}, bint{-1})), "2");
}

BOOST_AUTO_TEST_CASE(bitwise_and_shift_sign_semantics)
{
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{-5} & bint{-3}), "-1");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint{-5} & bint{3}), "1");

    bint bothNegative{-5};
    bothNegative |= bint{-3};
    BOOST_CHECK_EQUAL(bsv::to_dec(bothNegative), "7");
    bint mixedSigns{-5};
    mixedSigns |= bint{3};
    BOOST_CHECK_EQUAL(bsv::to_dec(mixedSigns), "-7");
    bint selfOr{-5};
    selfOr |= selfOr;
    BOOST_CHECK_EQUAL(bsv::to_dec(selfOr), "-5");

    bint shifted{-5};
    shifted >>= 1;
    BOOST_CHECK_EQUAL(bsv::to_dec(shifted), "-2");
    shifted <<= 2;
    BOOST_CHECK_EQUAL(bsv::to_dec(shifted), "-8");
    shifted <<= -1;
    BOOST_CHECK_EQUAL(bsv::to_dec(shifted), "-8");
}

BOOST_AUTO_TEST_CASE(script_number_serialization)
{
    struct Vector {
        int64_t value;
        std::vector<uint8_t> bytes;
    };
    const std::array vectors{
        Vector{0, {}},
        Vector{1, {0x01}},
        Vector{-1, {0x81}},
        Vector{127, {0x7f}},
        Vector{-127, {0xff}},
        Vector{128, {0x80, 0x00}},
        Vector{-128, {0x80, 0x80}},
        Vector{255, {0xff, 0x00}},
        Vector{-255, {0xff, 0x80}},
        Vector{256, {0x00, 0x01}},
        Vector{-256, {0x00, 0x81}},
        Vector{std::numeric_limits<int64_t>::max(),
               {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}},
        Vector{std::numeric_limits<int64_t>::min(),
               {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80}}
    };

    for(const auto& vector : vectors) {
        const bint number{vector.value};
        BOOST_CHECK(number.serialize() == vector.bytes);
        BOOST_CHECK_EQUAL(number.serialized_size(), vector.bytes.size());
        BOOST_CHECK_EQUAL(bsv::to_int64_t(bint::deserialize(vector.bytes)), vector.value);
    }

    BOOST_CHECK_EQUAL(bsv::to_dec(bint::deserialize(std::array<uint8_t, 1>{0x80})), "0");
    BOOST_CHECK_EQUAL(bsv::to_dec(bint::deserialize(std::array<uint8_t, 2>{0x01, 0x00})), "1");
    const bint huge{"340282366920938463463374607431768211455"};
    BOOST_CHECK(bint::deserialize(huge.serialize()) == huge);
}

BOOST_AUTO_TEST_SUITE_END()
