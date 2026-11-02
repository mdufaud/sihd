#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelMatch.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevFilter.hpp>
#include <sihd/util/Value.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::core;
using namespace sihd::util;
using Rule = DevFilter::Rule;

namespace
{
ChannelMatch::Comparison consume_comparison(FuzzedDataProvider & provider)
{
    return static_cast<ChannelMatch::Comparison>(
        provider.ConsumeIntegralInRange<int>(ChannelMatch::Equal, ChannelMatch::ByteXor));
}

std::string consume_word(FuzzedDataProvider & provider)
{
    std::string word(provider.ConsumeIntegralInRange<size_t>(1, 8), 'a');
    for (char & c : word)
        c = provider.ConsumeIntegralInRange<char>('a', 'z');
    return word;
}

void fuzz_text(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const std::vector<char> buffer = provider.ConsumeRemainingBytes<char>();

    Rule rule(comparison);
    if (!rule.parse(std::string_view(buffer.data(), buffer.size())))
        return;
    test::fuzz_check(rule.channel_match.comparison == comparison, "parsing a rule keeps its comparison");

    const Rule copy = rule;
    for (const Value & probe : {Value(0), Value(1), Value(-1), Value(1.5), Value(true), Value()})
        test::fuzz_check(copy.channel_match.match(probe) == rule.channel_match.match(probe),
                         "a copied rule matches like its source");
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
        default:
            return (input ^ reference) != 0;
    }
}

enum class WriteForm
{
    None,
    Value,
    IdxValue,
    IdxOnly,
};

void fuzz_structured(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const std::string channel_in = consume_word(provider);
    const std::string channel_out = consume_word(provider);
    const size_t trigger_idx = provider.ConsumeIntegral<size_t>();
    const int64_t trigger_value = provider.ConsumeIntegral<int64_t>();
    const bool trigger_with_idx = provider.ConsumeBool();
    const bool active = provider.ConsumeBool();
    const WriteForm form = static_cast<WriteForm>(provider.ConsumeIntegralInRange<int>(0, 3));
    const size_t write_idx = provider.ConsumeIntegral<size_t>();
    const int64_t write_value = provider.ConsumeIntegral<int64_t>();
    const bool with_match = provider.ConsumeBool();
    const int quarters = provider.ConsumeIntegralInRange<int>(0, 40);
    const bool with_delay = provider.ConsumeBool();

    std::string conf = fmt::format("in={};out={};trigger=", channel_in, channel_out);
    conf += trigger_with_idx ? fmt::format("{}:{}", trigger_idx, trigger_value) : fmt::format("{}", trigger_value);
    if (with_match)
        conf += fmt::format(";match={}", active);
    if (with_delay)
        conf += fmt::format(";delay={}", quarters / 4.0);
    if (form == WriteForm::Value)
        conf += fmt::format(";write={}", write_value);
    else if (form == WriteForm::IdxValue)
        conf += fmt::format(";write={}:{}", write_idx, write_value);
    else if (form == WriteForm::IdxOnly)
        conf += fmt::format(";write={}:", write_idx);

    const size_t expected_trigger_idx = trigger_with_idx ? trigger_idx : 0;
    Rule rule(comparison);
    test::fuzz_check(rule.parse(conf).has_value(), "a well formed rule parses");
    test::fuzz_check(rule.channel_in == channel_in, "the input channel is kept");
    test::fuzz_check(rule.channel_out == channel_out, "the output channel is kept");
    test::fuzz_check(rule.channel_match.comparison == comparison, "the comparison is kept");
    test::fuzz_check(rule.channel_match.idx == expected_trigger_idx, "the trigger index is kept, 0 without one");
    test::fuzz_check(rule.channel_match.value == trigger_value, "the trigger value is kept");
    test::fuzz_check(rule.channel_match.invert == (with_match && !active), "an inactive match inverts the trigger");
    test::fuzz_check(rule.nano_delay == std::chrono::milliseconds(with_delay ? quarters * 250 : 0),
                     "the delay is read in seconds");

    const bool writes_value = form == WriteForm::Value || form == WriteForm::IdxValue;
    test::fuzz_check(rule.write_same_value == !writes_value, "a rule writes its own value unless a value is given");
    const bool writes_idx = form == WriteForm::IdxValue || form == WriteForm::IdxOnly;
    test::fuzz_check(rule.write_idx == (writes_idx ? write_idx : expected_trigger_idx),
                     "a rule writes at its given index, else at the trigger index");
    if (writes_value)
        test::fuzz_check(rule.write_value == write_value, "the write value is kept");
}

void fuzz_refused(FuzzedDataProvider & provider)
{
    const std::string valid = "in=a;out=b;trigger=1:2";
    const std::array<std::string, 12> refused = {
        "out=b;trigger=1:2",
        "in=a;trigger=1:2",
        "in=a;out=b",
        "in=a;out=b;trigger=",
        "in=a;out=b;trigger=1:2:3",
        "in=a;out=b;trigger=x:2",
        valid + ";write=",
        valid + ";write=1:2:3",
        valid + ";write=x:2",
        valid + ";write=1:zz",
        valid + ";match=zz",
        valid + ";delay=zz",
    };
    Rule rule(static_cast<ChannelMatch::Comparison>(provider.ConsumeIntegralInRange<int>(1, 8)));
    for (const std::string & text : refused)
        test::fuzz_check(!Rule(rule).parse(text).has_value(), "a rule with a missing or malformed key is refused");
    test::fuzz_check(Rule(rule).parse(valid).has_value(), "the minimal rule parses");
}

void fuzz_run(FuzzedDataProvider & provider)
{
    constexpr size_t channel_size = 3;
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const size_t trigger_idx = provider.ConsumeIntegralInRange<size_t>(0, channel_size - 1);
    const int64_t trigger_value = provider.ConsumeIntegralInRange<int64_t>(0, 4);
    const bool active = provider.ConsumeBool();
    const WriteForm form = static_cast<WriteForm>(provider.ConsumeIntegralInRange<int>(0, 3));
    const size_t write_idx = provider.ConsumeIntegralInRange<size_t>(0, channel_size - 1);
    const int write_value = provider.ConsumeIntegralInRange<int>(0, 9);

    std::string conf = fmt::format("in=..in_channel;out=..out_channel;trigger={}:{};match={}",
                                   trigger_idx,
                                   trigger_value,
                                   active);
    if (form == WriteForm::Value)
        conf += fmt::format(";write={}", write_value);
    else if (form == WriteForm::IdxValue)
        conf += fmt::format(";write={}:{}", write_idx, write_value);
    else if (form == WriteForm::IdxOnly)
        conf += fmt::format(";write={}:", write_idx);
    const bool writes_value = form == WriteForm::Value || form == WriteForm::IdxValue;
    const size_t out_idx = form == WriteForm::IdxValue || form == WriteForm::IdxOnly ? write_idx : trigger_idx;

    Core core;
    DevFilter *filter = core.add_child<DevFilter>("filter");
    test::fuzz_check(
        filter->set_conf_str(std::string("filter_") + ChannelMatch::comparison_str(comparison), conf).has_value(),
        "a well formed filter is configured");
    core.add_channel("in_channel", "int", channel_size);
    core.add_channel("out_channel", "int", channel_size);
    // a rule with a delay brings a scheduler: the rule without a delay must still write at once
    if (provider.ConsumeBool())
    {
        test::fuzz_check(
            filter->set_conf_str("filter_equal", "in=..idle_in;out=..idle_out;trigger=0:0;delay=3600").has_value(),
            "a delayed rule is configured");
        core.add_channel("idle_in", "int", 1);
        core.add_channel("idle_out", "int", 1);
    }
    test::fuzz_check(core.init() && core.start(), "a filter with its channels starts");
    Channel *in = core.get_channel("in_channel").value_or(nullptr);
    Channel *out = core.get_channel("out_channel").value_or(nullptr);
    test::fuzz_check(in != nullptr && out != nullptr, "the channels are found");

    std::array<int, channel_size> in_model {};
    std::array<int, channel_size> out_model {};
    for (size_t ops = provider.ConsumeIntegralInRange<size_t>(1, 16); ops > 0; --ops)
    {
        const size_t idx = provider.ConsumeIntegralInRange<size_t>(0, channel_size - 1);
        const int value = provider.ConsumeIntegralInRange<int>(0, 4);
        in->write<int>(idx, value);

        // an unchanged value does not notify, so the rule is not evaluated
        if (in_model[idx] != value)
        {
            in_model[idx] = value;
            const int watched = in_model[trigger_idx];
            if (expected_match(comparison, watched, trigger_value) == active)
                out_model[out_idx] = writes_value ? write_value : watched;
        }
        for (size_t i = 0; i < channel_size; ++i)
            test::fuzz_check(out->read<int>(i) == out_model[i], "the output channel follows the filter rule");
    }
}

void fuzz_float_output(FuzzedDataProvider & provider)
{
    const bool write_same = provider.ConsumeBool();
    const bool float_value = provider.ConsumeBool();
    const std::string out_type = provider.PickValueInArray<std::string>({"int", "float", "double"});

    std::string conf = fmt::format("in=..in_channel;out=..out_channel;trigger=0:{}",
                                   write_same && float_value ? "1.5" : "1");
    if (!write_same)
        conf += fmt::format(";write={}", float_value ? "2.5" : "2");

    Core core;
    DevFilter *filter = core.add_child<DevFilter>("filter");
    test::fuzz_check(filter->set_conf_str("filter_equal", conf).has_value(), "a well formed filter is configured");
    core.add_channel("in_channel", "double", 1);
    core.add_channel("out_channel", out_type, 1);
    test::fuzz_check((core.init() && core.start()) == (out_type != "int" || !float_value),
                     "only a floating output takes a float value");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 4))
    {
        case 0:
            fuzz_text(provider);
            break;
        case 1:
            fuzz_structured(provider);
            break;
        case 2:
            fuzz_refused(provider);
            break;
        case 3:
            fuzz_float_output(provider);
            break;
        default:
            fuzz_run(provider);
            break;
    }
    return 0;
}
