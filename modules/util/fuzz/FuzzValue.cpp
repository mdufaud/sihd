#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <sihd/util/Value.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr size_t max_text_size = 128;
// str() prints six decimals and the reparse stays exact below this magnitude
constexpr double max_printed_float = 1e9;
constexpr double printed_float_tolerance = 1e-6;

void check_float_round_trip(const Value & value)
{
    const double number = value.to_double();
    if (!std::isfinite(number) || std::fabs(number) >= max_printed_float)
        return;
    const Value reparsed = Value::from_any_string(value.str());
    test::fuzz_check(reparsed.is_float(), "a printed float parses as a float");
    test::fuzz_check(std::fabs(reparsed.to_double() - number) <= printed_float_tolerance,
                     "a printed float parses back within its six decimals");
}

void check_str_round_trip(const Value & value)
{
    if (value.empty())
        return;
    if (value.is_float())
    {
        check_float_round_trip(value);
        return;
    }
    const std::string text = (value.type == TYPE_ULONG ? "u" : "") + value.str();
    const Value reparsed = Value::from_any_string(text);
    test::fuzz_check(!reparsed.empty(), "str of a parsed value parses again");
    test::fuzz_check(reparsed.compare(value) == 0, "str of a parsed value parses to an equal value");
}

void fuzz_raw_text(FuzzedDataProvider & provider)
{
    const test::ExactText text(provider.ConsumeBytesAsString(max_text_size));
    check_str_round_trip(Value::from_bool_string(text));
    check_str_round_trip(Value::from_char_string(text));
    check_str_round_trip(Value::from_int_string(text));
    check_str_round_trip(Value::from_float_string(text));
    check_str_round_trip(Value::from_any_string(text));
}

void fuzz_integer_text(FuzzedDataProvider & provider)
{
    const int64_t signed_value = provider.ConsumeIntegral<int64_t>();
    const uint64_t unsigned_value = provider.ConsumeIntegral<uint64_t>();
    const int64_t non_negative = static_cast<int64_t>(unsigned_value >> 1);

    const Value decimal = Value::from_any_string(std::to_string(signed_value));
    test::fuzz_check(!decimal.empty() && decimal.compare(signed_value) == 0, "decimal text parses to its integer");

    char hexadecimal[24];
    std::snprintf(hexadecimal, sizeof(hexadecimal), "0x%llx", static_cast<unsigned long long>(non_negative));
    const Value from_hexadecimal = Value::from_any_string(hexadecimal);
    test::fuzz_check(!from_hexadecimal.empty() && from_hexadecimal.compare(non_negative) == 0,
                     "0x text parses to its integer");

    const Value from_binary = Value::from_any_string("0b" + std::bitset<64>(non_negative).to_string());
    test::fuzz_check(!from_binary.empty() && from_binary.compare(non_negative) == 0, "0b text parses to its integer");

    const Value unsigned_decimal = Value::from_any_string("u" + std::to_string(unsigned_value));
    test::fuzz_check(unsigned_decimal.type == TYPE_ULONG && unsigned_decimal.compare(unsigned_value) == 0,
                     "u text parses to its unsigned integer");
}

void fuzz_float_text(FuzzedDataProvider & provider)
{
    const bool negative = provider.ConsumeBool();
    const bool single = provider.ConsumeBool();
    const uint32_t integer_part = provider.ConsumeIntegralInRange<uint32_t>(0, 1000000);
    const uint32_t fraction = provider.ConsumeIntegralInRange<uint32_t>(0, 999999);

    char number[32];
    std::snprintf(number, sizeof(number), "%s%u.%06u", negative ? "-" : "", integer_part, fraction);
    const Value parsed = Value::from_any_string(single ? std::string(number) + "f" : std::string(number));
    if (single)
        test::fuzz_check(parsed.type == TYPE_FLOAT && parsed.data.f == std::strtof(number, nullptr),
                         "text with an f suffix parses to the nearest float");
    else
        test::fuzz_check(parsed.type == TYPE_DOUBLE && parsed.data.d == std::strtod(number, nullptr),
                         "text with a dot parses to the nearest double");
}

constexpr int64_t max_exact_integer = int64_t(1) << 53;

struct Operand
{
        Value value;
        long double exact;
};

Operand consume_operand(FuzzedDataProvider & provider, bool exact_only)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 5))
    {
        case 0:
        {
            const int64_t signed_number = exact_only ? provider.ConsumeIntegralInRange<int64_t>(-max_exact_integer,
                                                                                                max_exact_integer)
                                                     : provider.ConsumeIntegral<int64_t>();
            return {Value(signed_number), static_cast<long double>(signed_number)};
        }
        case 1:
        {
            const uint64_t unsigned_number = exact_only
                                                 ? provider.ConsumeIntegralInRange<uint64_t>(0, max_exact_integer)
                                                 : provider.ConsumeIntegral<uint64_t>();
            return {Value(unsigned_number), static_cast<long double>(unsigned_number)};
        }
        case 2:
        {
            const double number = provider.ConsumeFloatingPoint<double>();
            return {Value(number), number};
        }
        case 3:
        {
            // around the zero where the signed and unsigned comparison branches meet
            const int64_t signed_number = provider.ConsumeIntegralInRange<int64_t>(-1, 1);
            return {Value(signed_number), static_cast<long double>(signed_number)};
        }

        case 4:
        {
            const uint64_t unsigned_number = provider.ConsumeIntegralInRange<uint64_t>(0, 1);
            return {Value(unsigned_number), static_cast<long double>(unsigned_number)};
        }

        default:
        {
            // a half step off an integer: the comparison must not round it away
            const int64_t base = provider.ConsumeIntegralInRange<int64_t>(-1000, 1000);
            const double number = static_cast<double>(base) + (provider.ConsumeBool() ? 0.5 : -0.5);
            return {Value(number), number};
        }
    }
}

int sign_of(const long double left, const long double right)
{
    return (left > right) - (left < right);
}

void fuzz_compare(FuzzedDataProvider & provider)
{
    const bool exact_only = provider.ConsumeBool();
    const Operand left = consume_operand(provider, exact_only);
    const Operand right = consume_operand(provider, exact_only);
    const bool any_float = left.value.is_float() || right.value.is_float();
    if (any_float && !exact_only)
        return;

    const int expected = sign_of(left.exact, right.exact);
    test::fuzz_check(sign_of(left.value.compare(right.value), 0) == expected, "compare orders two values");
    test::fuzz_check(sign_of(right.value.compare(left.value), 0) == -expected, "compare is antisymmetric");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_raw_text(provider);
            break;
        case 1:
            fuzz_integer_text(provider);
            break;
        case 2:
            fuzz_float_text(provider);
            break;
        default:
            fuzz_compare(provider);
            break;
    }
    return 0;
}
