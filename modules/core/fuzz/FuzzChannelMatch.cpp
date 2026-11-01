#include <cstddef>
#include <cstdint>
#include <format>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/core/ChannelMatch.hpp>
#include <sihd/util/Value.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::core;
using namespace sihd::util;

namespace
{
ChannelMatch::Comparison consume_comparison(FuzzedDataProvider & provider)
{
    return static_cast<ChannelMatch::Comparison>(
        provider.ConsumeIntegralInRange<int>(ChannelMatch::Equal, ChannelMatch::ByteXor));
}

bool expected_match(ChannelMatch::Comparison comparison, int64_t input, int64_t reference)
{
    switch (comparison)
    {
        case ChannelMatch::Equal:
            return input == reference;
        case ChannelMatch::Superior:
            return input > reference;
        case ChannelMatch::SuperiorEqual:
            return input >= reference;
        case ChannelMatch::Inferior:
            return input < reference;
        case ChannelMatch::InferiorEqual:
            return input <= reference;
        case ChannelMatch::ByteAnd:
            return (input & reference) != 0;
        case ChannelMatch::ByteOr:
            return (input | reference) != 0;
        case ChannelMatch::ByteXor:
            return (input ^ reference) != 0;
        default:
            return false;
    }
}

void check_fields(const ChannelMatch & match, ChannelMatch::Comparison comparison, size_t idx, int64_t value)
{
    test::fuzz_check(match.comparison == comparison, "the comparison is kept");
    test::fuzz_check(match.idx == idx, "the index is kept");
    test::fuzz_check(match.value == value, "the value is kept");
}

void check_reusable(const ChannelMatch & match)
{
    ChannelMatch inverted = match;
    inverted.invert = !match.invert;
    for (const Value & probe : {Value(0), Value(-1), Value(255), Value(1.5), Value(1.5f), Value(true), Value()})
    {
        const bool flipped = match.match(probe) != inverted.match(probe);
        test::fuzz_check(flipped == (match.comparison != ChannelMatch::None && !probe.empty()),
                         "invert negates the result of a match");
    }
}

void fuzz_text(FuzzedDataProvider & provider)
{
    const std::vector<char> buffer = provider.ConsumeRemainingBytes<char>();
    const std::string_view text(buffer.data(), buffer.size());

    ChannelMatch parsed;
    if (parsed.parse(text))
    {
        test::fuzz_check(parsed.comparison != ChannelMatch::None, "a parsed match has a comparison");
        check_reusable(parsed);
    }

    ChannelMatch triggered(ChannelMatch::Superior, Value(3), 1);
    if (triggered.parse_trigger(text))
        check_reusable(triggered);
    test::fuzz_check(triggered.comparison == ChannelMatch::Superior, "a trigger leaves the comparison unchanged");

    const ChannelMatch::Comparison comparison = ChannelMatch::comparison_from_str(text);
    test::fuzz_check(comparison == ChannelMatch::None || text == ChannelMatch::comparison_str(comparison),
                     "a comparison name is the string of its comparison");
}

void fuzz_parse_round_trip(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const size_t idx = provider.ConsumeIntegral<size_t>();
    const int64_t value = provider.ConsumeIntegral<int64_t>();
    const bool invert = provider.ConsumeBool();

    ChannelMatch match;
    const std::string conf = std::format("cmp={};idx={};value={};invert={}",
                                         ChannelMatch::comparison_str(comparison),
                                         idx,
                                         value,
                                         invert);
    test::fuzz_check(match.parse(conf).has_value(), "a well formed match parses");
    check_fields(match, comparison, idx, value);
    test::fuzz_check(match.invert == invert, "the inversion is kept");
}

void fuzz_trigger_round_trip(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const size_t idx = provider.ConsumeIntegral<size_t>();
    const int64_t value = provider.ConsumeIntegral<int64_t>();

    ChannelMatch indexed(comparison, Value(0));
    test::fuzz_check(indexed.parse_trigger(std::format("{}:{}", idx, value)).has_value(), "an indexed trigger parses");
    check_fields(indexed, comparison, idx, value);

    ChannelMatch plain(comparison, Value(0));
    test::fuzz_check(plain.parse_trigger(std::format("{}", value)).has_value(), "a plain trigger parses");
    check_fields(plain, comparison, 0, value);
}

void fuzz_match(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const int64_t reference = provider.ConsumeIntegral<int64_t>();
    const int64_t input = provider.ConsumeIntegral<int64_t>();

    ChannelMatch match(comparison, Value(reference));
    match.invert = provider.ConsumeBool();
    test::fuzz_check(match.match(Value(input)) == (expected_match(comparison, input, reference) != match.invert),
                     "a match applies its comparison then its inversion");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_text(provider);
            break;
        case 1:
            fuzz_parse_round_trip(provider);
            break;
        case 2:
            fuzz_trigger_round_trip(provider);
            break;
        default:
            fuzz_match(provider);
            break;
    }
    return 0;
}
