#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include <sihd/core/Channel.hpp>
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

template <typename Input, typename Reference>
bool expected_match(ChannelMatch::Comparison comparison, Input input, Reference reference)
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
        default:
            break;
    }
    if constexpr (std::is_integral_v<Input> && std::is_integral_v<Reference>)
    {
        switch (comparison)
        {
            case ChannelMatch::ByteAnd:
                return (input & reference) != 0;
            case ChannelMatch::ByteOr:
                return (input | reference) != 0;
            case ChannelMatch::ByteXor:
                return (input ^ reference) != 0;
            default:
                break;
        }
    }
    return false;
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
    const std::string conf = fmt::format("cmp={};idx={};value={};invert={}",
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
    test::fuzz_check(indexed.parse_trigger(fmt::format("{}:{}", idx, value)).has_value(), "an indexed trigger parses");
    check_fields(indexed, comparison, idx, value);

    ChannelMatch plain(comparison, Value(0));
    test::fuzz_check(plain.parse_trigger(fmt::format("{}", value)).has_value(), "a plain trigger parses");
    check_fields(plain, comparison, 0, value);
}

void fuzz_match(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = static_cast<ChannelMatch::Comparison>(
        provider.ConsumeIntegralInRange<int>(ChannelMatch::None, ChannelMatch::ByteXor));
    const int64_t reference = provider.ConsumeIntegral<int64_t>();
    const int64_t input = provider.ConsumeIntegral<int64_t>();

    ChannelMatch match(comparison, Value(reference));
    match.invert = provider.ConsumeBool();
    const bool expected = comparison != ChannelMatch::None
                          && expected_match(comparison, input, reference) != match.invert;
    test::fuzz_check(match.match(Value(input)) == expected, "a match applies its comparison then its inversion");
    test::fuzz_check(!match.match(Value()), "an empty value never matches, inverted or not");
    test::fuzz_check(!match.match(static_cast<const Channel *>(nullptr)), "no channel never matches");
}

void fuzz_match_float(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = static_cast<ChannelMatch::Comparison>(
        provider.ConsumeIntegralInRange<int>(ChannelMatch::Equal, ChannelMatch::InferiorEqual));
    const double reference = provider.ConsumeIntegralInRange<int>(-8, 8) / 4.0;
    const double input = provider.ConsumeIntegralInRange<int>(-8, 8) / 4.0;

    ChannelMatch match(comparison, Value(reference));
    match.invert = provider.ConsumeBool();
    test::fuzz_check(match.match(Value(input)) == (expected_match(comparison, input, reference) != match.invert),
                     "a float match applies its comparison then its inversion");
}

std::optional<ChannelMatch> parse_trigger_from(const ChannelMatch & base, const std::string & text)
{
    ChannelMatch match = base;
    if (!match.parse_trigger(text).has_value())
        return std::nullopt;
    return match;
}

void fuzz_trigger_forms(FuzzedDataProvider & provider)
{
    const ChannelMatch base(ChannelMatch::Equal,
                            Value(provider.ConsumeIntegral<int64_t>()),
                            provider.ConsumeIntegral<size_t>());
    const size_t idx = provider.ConsumeIntegral<size_t>();
    const int64_t value = provider.ConsumeIntegral<int64_t>();
    const std::string idx_text = std::to_string(idx);
    const std::string value_text = std::to_string(value);

    const auto full = parse_trigger_from(base, idx_text + ":" + value_text);
    test::fuzz_check(full.has_value() && full->idx == idx && full->value == value, "idx:value sets both");

    // suspected bug: Splitter drops the leading empty field, so ":value" resets idx to 0 instead of keeping it
    const auto value_only = parse_trigger_from(base, ":" + value_text);
    test::fuzz_check(value_only.has_value() && value_only->value == value, ":value sets the value");

    const auto idx_only = parse_trigger_from(base, idx_text + ":");
    test::fuzz_check(idx_only.has_value() && idx_only->idx == idx && idx_only->value == base.value,
                     "idx: sets the index and keeps the value");

    const auto plain = parse_trigger_from(base, value_text);
    test::fuzz_check(plain.has_value() && plain->idx == 0 && plain->value == value,
                     "value alone sets it and resets the index");

    const std::vector<std::string> malformed = {"",
                                                ":",
                                                idx_text + ":" + value_text + ":" + value_text,
                                                "x" + idx_text + ":" + value_text,
                                                idx_text + ":zz"};
    for (const std::string & text : malformed)
    {
        ChannelMatch kept = base;
        test::fuzz_check(!kept.parse_trigger(text).has_value(), "a malformed trigger is refused");
        test::fuzz_check(kept.idx == base.idx && kept.value == base.value, "a refused trigger keeps the previous one");
    }
}

void fuzz_parse_keys(FuzzedDataProvider & provider)
{
    ChannelMatch previous(ChannelMatch::Superior,
                          Value(provider.ConsumeIntegral<int64_t>()),
                          provider.ConsumeIntegral<size_t>());
    previous.invert = provider.ConsumeBool();

    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const size_t idx = provider.ConsumeIntegral<size_t>();
    const int64_t value = provider.ConsumeIntegral<int64_t>();
    const bool invert = provider.ConsumeBool();
    const bool with_idx = provider.ConsumeBool();
    const bool with_value = provider.ConsumeBool();
    const bool with_invert = provider.ConsumeBool();

    std::string conf = fmt::format("cmp={}", ChannelMatch::comparison_str(comparison));
    if (with_idx)
        conf += fmt::format(";idx={}", idx);
    if (with_value)
        conf += fmt::format(";value={}", value);
    if (with_invert)
        conf += fmt::format(";invert={}", invert);

    ChannelMatch match = previous;
    test::fuzz_check(match.parse(conf).has_value(), "a match with only some keys parses");
    check_fields(match, comparison, with_idx ? idx : previous.idx, with_value ? value : previous.value.data.n);
    test::fuzz_check(match.invert == (with_invert ? invert : previous.invert),
                     "an absent key keeps its previous state");

    for (const std::string_view bad : {"idx=1;value=2",
                                       "cmp=bogus;idx=1",
                                       "cmp=none",
                                       "cmp=equal;idx=zz",
                                       "cmp=equal;value=zz",
                                       "cmp=equal;invert=zz"})
    {
        ChannelMatch refused = previous;
        test::fuzz_check(!refused.parse(bad).has_value(), "a match with a bad key is refused");
        check_fields(refused, previous.comparison, previous.idx, previous.value.data.n);
        test::fuzz_check(refused.invert == previous.invert, "a refused match keeps its previous state");
    }
}

template <typename T>
void check_channel_match(FuzzedDataProvider & provider, const char *type, size_t size, bool floating)
{
    Channel channel("chan", type, size);
    std::vector<T> elements(size);
    for (size_t i = 0; i < size; ++i)
    {
        elements[i] = floating ? static_cast<T>(provider.ConsumeIntegralInRange<int>(-8, 8) / 4.0)
                               : static_cast<T>(provider.ConsumeIntegralInRange<int>(-8, 8));
        test::fuzz_check(channel.write(i, elements[i]), "an element is written inside the channel");
    }

    const size_t idx = provider.ConsumeIntegralInRange<size_t>(0, size + 2);
    const bool float_reference = provider.ConsumeBool();
    const bool bytes_allowed = !floating && !float_reference;
    const ChannelMatch::Comparison comparison = static_cast<ChannelMatch::Comparison>(
        provider.ConsumeIntegralInRange<int>(ChannelMatch::None,
                                             bytes_allowed ? ChannelMatch::ByteXor : ChannelMatch::InferiorEqual));
    const int steps = provider.ConsumeIntegralInRange<int>(-8, 8);
    ChannelMatch match = float_reference ? ChannelMatch(comparison, Value(steps / 4.0), idx)
                                         : ChannelMatch(comparison, Value(steps), idx);
    match.invert = provider.ConsumeBool();

    bool expected = false;
    if (comparison != ChannelMatch::None && idx < size)
        expected = (float_reference ? expected_match(comparison, elements[idx], steps / 4.0)
                                    : expected_match(comparison, elements[idx], steps))
                   != match.invert;
    test::fuzz_check(match.match(&channel) == expected, "a channel matches the element at its index");

    const bool verified = comparison != ChannelMatch::None && idx < size && (floating || !float_reference);
    test::fuzz_check(match.verify(&channel) == verified,
                     "a match is valid for a channel when set, in range and typed like it");
    test::fuzz_check(!match.verify(nullptr), "no channel is never valid");
}

void fuzz_channel(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(1, 6);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            check_channel_match<int>(provider, "int", size, false);
            break;
        case 1:
            check_channel_match<int64_t>(provider, "long", size, false);
            break;
        case 2:
            check_channel_match<int16_t>(provider, "short", size, false);
            break;
        default:
            check_channel_match<double>(provider, "double", size, true);
            break;
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 7))
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
        case 3:
            fuzz_match(provider);
            break;
        case 4:
            fuzz_match_float(provider);
            break;
        case 5:
            fuzz_trigger_forms(provider);
            break;
        case 6:
            fuzz_parse_keys(provider);
            break;
        default:
            fuzz_channel(provider);
            break;
    }
    return 0;
}
