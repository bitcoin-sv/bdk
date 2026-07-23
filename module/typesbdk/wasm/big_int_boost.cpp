/* Header-only big-integer backend owned by the constrained WASM module. */
#include <big_int.h>

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

using boost::multiprecision::cpp_int;

struct bignum_st {
    cpp_int value;
};

namespace {

cpp_int Magnitude(const cpp_int& value)
{
    return value < 0 ? -value : value;
}

cpp_int SignedMagnitude(cpp_int magnitude, bool negative)
{
    return negative && magnitude != 0 ? -magnitude : magnitude;
}

cpp_int ParseDecimal(const std::string& text)
{
    size_t position = 0;
    bool negative = false;
    if(position < text.size() && text[position] == '-') {
        negative = true;
        ++position;
    }
    if(position == text.size() || text[position] < '0' || text[position] > '9')
        throw bsv::big_int_error{};

    cpp_int result = 0;
    while(position < text.size() && text[position] >= '0' && text[position] <= '9') {
        result *= 10;
        result += text[position] - '0';
        ++position;
    }
    return SignedMagnitude(std::move(result), negative);
}

std::vector<uint8_t> MagnitudeBytes(const cpp_int& value)
{
    std::vector<uint8_t> bytes;
    const cpp_int magnitude = Magnitude(value);
    if(magnitude != 0)
        export_bits(magnitude, std::back_inserter(bytes), 8, true);
    return bytes;
}

cpp_int FromMagnitudeBytes(const std::vector<uint8_t>& bytes)
{
    cpp_int value = 0;
    import_bits(value, bytes.begin(), bytes.end(), 8, true);
    return value;
}

template<typename T>
T CheckedConvert(const cpp_int& value)
{
    static const cpp_int minimum = std::numeric_limits<T>::min();
    static const cpp_int maximum = std::numeric_limits<T>::max();
    if(value < minimum || value > maximum)
        throw bsv::big_int_error{};
    return value.convert_to<T>();
}

std::string HexMagnitude(const cpp_int& value)
{
    static constexpr char digits[] = "0123456789ABCDEF";
    const auto bytes = MagnitudeBytes(value);
    if(bytes.empty()) return "0";
    std::string result;
    result.reserve(bytes.size() * 2 + (value < 0 ? 1 : 0));
    if(value < 0) result.push_back('-');
    for(const uint8_t byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

} // namespace

void bsv::bint::empty_bn_deleter::operator()(bignum_st* value) const
{
    delete value;
}

bsv::bint::bint()
    : bint{0}
{}

bsv::bint::bint(int value)
    : value_{new bignum_st{cpp_int{value}}, empty_bn_deleter{}}
{}

bsv::bint::bint(int64_t value)
    : value_{new bignum_st{cpp_int{value}}, empty_bn_deleter{}}
{}

bsv::bint::bint(size_t value)
    : value_{new bignum_st{cpp_int{value}}, empty_bn_deleter{}}
{}

bsv::bint::bint(const std::string& value)
    : value_{new bignum_st{ParseDecimal(value)}, empty_bn_deleter{}}
{}

bsv::bint::bint(const bint& other)
    : value_{new bignum_st{other.value_->value}, empty_bn_deleter{}}
{}

bsv::bint& bsv::bint::operator=(const bint& other)
{
    bint copy{other};
    swap(copy);
    return *this;
}

void bsv::bint::swap(bint& other) noexcept
{
    value_.swap(other.value_);
}

std::strong_ordering bsv::operator<=>(const bint& left, const bint& right)
{
    if(left.value_->value < right.value_->value) return std::strong_ordering::less;
    if(left.value_->value > right.value_->value) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

bool bsv::operator==(const bint& left, const bint& right)
{
    return left.value_->value == right.value_->value;
}

bsv::bint& bsv::bint::operator+=(const bint& other)
{
    value_->value += other.value_->value;
    return *this;
}

bsv::bint& bsv::bint::operator-=(const bint& other)
{
    value_->value -= other.value_->value;
    return *this;
}

bsv::bint& bsv::bint::operator*=(const bint& other)
{
    value_->value *= other.value_->value;
    return *this;
}

bsv::bint& bsv::bint::operator/=(const bint& other)
{
    if(other.value_->value == 0) throw big_int_error{};
    value_->value /= other.value_->value;
    return *this;
}

bsv::bint& bsv::bint::operator%=(const bint& other)
{
    if(other.value_->value == 0) throw big_int_error{};
    value_->value %= other.value_->value;
    return *this;
}

bsv::bint& bsv::bint::operator&=(const bint& other)
{
    const bool negative = value_->value < 0 && other.value_->value < 0;
    value_->value = SignedMagnitude(
        Magnitude(value_->value) & Magnitude(other.value_->value), negative);
    return *this;
}

bsv::bint& bsv::bint::operator|=(const bint& other)
{
    if(this == &other) return *this;
    const bool negative = (value_->value < 0) != (other.value_->value < 0);
    value_->value = SignedMagnitude(
        Magnitude(value_->value) | Magnitude(other.value_->value), negative);
    return *this;
}

bsv::bint& bsv::bint::operator<<=(int shift)
{
    if(shift <= 0) return *this;
    const bool negative = value_->value < 0;
    value_->value = SignedMagnitude(Magnitude(value_->value) << shift, negative);
    return *this;
}

bsv::bint& bsv::bint::operator<<=(const bint& shift)
{
    if(shift <= 0) return *this;
    if(shift > INT_MAX) throw big_int_error{};
    return *this <<= static_cast<int>(to_int64_t(shift));
}

bsv::bint& bsv::bint::operator>>=(int shift)
{
    if(shift <= 0) return *this;
    const bool negative = value_->value < 0;
    value_->value = SignedMagnitude(Magnitude(value_->value) >> shift, negative);
    return *this;
}

bsv::bint& bsv::bint::operator>>=(const bint& shift)
{
    if(shift <= 0) return *this;
    if(shift > INT_MAX) throw big_int_error{};
    return *this >>= static_cast<int>(to_int64_t(shift));
}

bsv::bint bsv::bint::operator-() const
{
    bint result{*this};
    result.negate();
    return result;
}

uint8_t bsv::bint::lsb() const
{
    return static_cast<uint8_t>((Magnitude(value_->value) & 0xff).convert_to<unsigned int>());
}

void bsv::bint::negate()
{
    value_->value = -value_->value;
}

void bsv::bint::mask_bits(int bits)
{
    if(bits < 0) throw big_int_error{};
    const bool negative = value_->value < 0;
    const cpp_int mask = bits == 0 ? cpp_int{0} : (cpp_int{1} << bits) - 1;
    value_->value = SignedMagnitude(Magnitude(value_->value) & mask, negative);
}

int bsv::bint::size_bits() const
{
    const cpp_int magnitude = Magnitude(value_->value);
    return magnitude == 0 ? 0 : static_cast<int>(boost::multiprecision::msb(magnitude) + 1);
}

int bsv::bint::size_bytes() const
{
    return (size_bits() + 7) / 8;
}

bsv::bint::buffer_type bsv::bint::to_bin() const
{
    return MagnitudeBytes(value_->value);
}

std::ostream& bsv::operator<<(std::ostream& stream, const bint& value)
{
    return stream << value.value_->value;
}

bool bsv::is_negative(const bint& value)
{
    return value.value_->value < 0;
}

bsv::bint bsv::abs(const bint& value)
{
    return is_negative(value) ? -value : value;
}

std::string bsv::to_dec(const bint& value)
{
    return value.value_->value.str();
}

std::string bsv::to_hex(const bint& value)
{
    return HexMagnitude(value.value_->value);
}

int64_t bsv::to_int64_t(const bint& value)
{
    return CheckedConvert<int64_t>(value.value_->value);
}

long bsv::to_long(const bint& value)
{
    try {
        return CheckedConvert<long>(value.value_->value);
    }
    catch(const big_int_error&) {
        return -1;
    }
}

size_t bsv::to_size_t_limited(const bint& value)
{
    return static_cast<size_t>(to_long(value));
}

std::vector<uint8_t> bsv::bint::serialize() const
{
    auto result = MagnitudeBytes(value_->value);
    if(result.empty()) return result;
    if((result.front() & 0x80) != 0) {
        result.insert(result.begin(), value_->value < 0 ? 0x80 : 0x00);
    }
    else if(value_->value < 0) {
        result.front() |= 0x80;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

size_t bsv::bint::serialized_size() const
{
    const int bytes = size_bytes();
    if(bytes == 0) return 0;
    const auto magnitude = MagnitudeBytes(value_->value);
    return static_cast<size_t>(bytes + ((magnitude.front() & 0x80) != 0 ? 1 : 0));
}

bsv::bint bsv::bint::deserialize(std::span<const uint8_t> bytes)
{
    if(bytes.empty()) return bint{0};
    std::vector<uint8_t> magnitude(bytes.rbegin(), bytes.rend());
    const bool negative = (magnitude.front() & 0x80) != 0;
    magnitude.front() &= 0x7f;
    bint result;
    result.value_->value = SignedMagnitude(FromMagnitudeBytes(magnitude), negative);
    return result;
}

bsv::bint bsv::pow(const bint& base, const bint& exponent)
{
    // BN_exp uses the exponent's magnitude. Preserve that behavior so this
    // backend remains interchangeable even for negative inputs.
    cpp_int power = Magnitude(exponent.value_->value);
    cpp_int factor = base.value_->value;
    cpp_int result = 1;
    while(power != 0) {
        if((power & 1) != 0) result *= factor;
        power >>= 1;
        if(power != 0) factor *= factor;
    }
    bint value;
    value.value_->value = std::move(result);
    return value;
}
