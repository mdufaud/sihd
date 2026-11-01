#include <cstddef>
#include <cstdint>
#include <format>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/core/ChannelMatch.hpp>
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

void fuzz_structured(FuzzedDataProvider & provider)
{
    const ChannelMatch::Comparison comparison = consume_comparison(provider);
    const std::string channel_in = consume_word(provider);
    const std::string channel_out = consume_word(provider);
    const size_t trigger_idx = provider.ConsumeIntegral<size_t>();
    const int64_t trigger_value = provider.ConsumeIntegral<int64_t>();
    const bool active = provider.ConsumeBool();
    const bool with_write = provider.ConsumeBool();
    const size_t write_idx = provider.ConsumeIntegral<size_t>();
    const int64_t write_value = provider.ConsumeIntegral<int64_t>();

    std::string conf = std::format("in={};out={};trigger={}:{};match={}",
                                   channel_in,
                                   channel_out,
                                   trigger_idx,
                                   trigger_value,
                                   active);
    if (with_write)
        conf += std::format(";write={}:{}", write_idx, write_value);

    Rule rule(comparison);
    test::fuzz_check(rule.parse(conf).has_value(), "a well formed rule parses");
    test::fuzz_check(rule.channel_in == channel_in, "the input channel is kept");
    test::fuzz_check(rule.channel_out == channel_out, "the output channel is kept");
    test::fuzz_check(rule.channel_match.comparison == comparison, "the comparison is kept");
    test::fuzz_check(rule.channel_match.idx == trigger_idx, "the trigger index is kept");
    test::fuzz_check(rule.channel_match.value == trigger_value, "the trigger value is kept");
    test::fuzz_check(rule.channel_match.invert == !active, "an inactive match inverts the trigger");
    test::fuzz_check(rule.write_same_value == !with_write, "a rule without write copies the input value");
    if (with_write)
    {
        test::fuzz_check(rule.write_idx == write_idx, "the write index is kept");
        test::fuzz_check(rule.write_value == write_value, "the write value is kept");
    }
    else
        test::fuzz_check(rule.write_idx == trigger_idx, "a rule without write targets the trigger index");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_structured(provider);
    else
        fuzz_text(provider);
    return 0;
}
