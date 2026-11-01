#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::json;

namespace
{
constexpr size_t max_brackets = 256;
constexpr std::string_view spaces[] = {"", " ", "\n", "\t "};
constexpr std::string_view string_pieces[] = {
    "a",
    "b",
    " ",
    "\xc3\xa9",
    "\\u00e9",
    "\\n",
    "\\\"",
    "\\\\",
    "\\/",
    "\\ud83d\\ude00",
    "\\u0000",
};

void check(bool ok, const char *property)
{
    if (ok)
        return;
    std::fprintf(stderr, "fuzz property violated: %s\n", property);
    std::abort();
}

std::string consume_spaces(FuzzedDataProvider & provider)
{
    return std::string(spaces[provider.ConsumeIntegralInRange<size_t>(0, std::size(spaces) - 1)]);
}

std::string json_string(FuzzedDataProvider & provider)
{
    std::string text = "\"";
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 5); i > 0; --i)
        text += string_pieces[provider.ConsumeIntegralInRange<size_t>(0, std::size(string_pieces) - 1)];
    return text + "\"";
}

std::string json_number(FuzzedDataProvider & provider)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return std::to_string(provider.ConsumeIntegral<int64_t>());
        case 1:
            return std::to_string(provider.ConsumeIntegralInRange<int>(-999, 999)) + "."
                   + std::to_string(provider.ConsumeIntegralInRange<int>(0, 999));
        default:
            return std::to_string(provider.ConsumeIntegralInRange<int>(1, 99999)) + "e"
                   + std::to_string(provider.ConsumeIntegralInRange<int>(-30, 30));
    }
}

std::string json_list(FuzzedDataProvider & provider, const std::vector<std::string> & items)
{
    std::string text;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
            text += consume_spaces(provider) + "," + consume_spaces(provider);
        text += items[i];
    }
    return text;
}

std::string json_member(FuzzedDataProvider & provider, const std::string & key, const std::string & value)
{
    return "\"" + key + "\"" + consume_spaces(provider) + ":" + consume_spaces(provider) + value;
}

std::string json_value(FuzzedDataProvider & provider, int depth)
{
    switch (provider.ConsumeIntegralInRange<int>(0, depth > 0 ? 5 : 3))
    {
        case 0:
            return "null";
        case 1:
            return provider.ConsumeBool() ? "true" : "false";
        case 2:
            return json_number(provider);
        case 3:
            return json_string(provider);
        case 4:
        {
            std::vector<std::string> items;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
                items.push_back(json_value(provider, depth - 1));
            return "[" + consume_spaces(provider) + json_list(provider, items) + "]";
        }
        default:
        {
            std::vector<std::string> members;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
                members.push_back(
                    json_member(provider, "k" + std::to_string(members.size()), json_value(provider, depth - 1)));
            return "{" + consume_spaces(provider) + json_list(provider, members) + "}";
        }
    }
}

void fuzz_utils_raw(FuzzedDataProvider & provider)
{
    const size_t limit = provider.ConsumeIntegralInRange<size_t>(1, 8);
    const std::string key = provider.ConsumeRandomLengthString(8);
    const std::vector<char> text = provider.ConsumeRemainingBytes<char>();
    const std::string_view data(text.data(), text.size());

    size_t calls = 0;
    const std::expected<size_t, std::string> walked = utils::for_each(data, [&](Json) {
        ++calls;
        return calls < limit;
    });
    if (walked.has_value())
    {
        check(*walked == calls, "for_each returns the number of elements it delivered");
        check(calls <= limit, "for_each stops when the callback says so");
        check(calls <= data.size(), "an element takes at least one byte");
    }
    else
        check(!walked.error().empty(), "a for_each failure explains itself");

    const std::expected<Json, std::string> found = utils::find_first(data, key);
    check(found.has_value() || !found.error().empty(), "a find_first failure explains itself");

    const size_t brackets = static_cast<size_t>(std::ranges::count(data, '[') + std::ranges::count(data, '{'));
    const std::expected<Json, std::string> document = Json::parse(data);
    if (document.has_value() && document->is_array() && brackets <= max_brackets)
    {
        size_t all = 0;
        const std::expected<size_t, std::string> full = utils::for_each(data, [&](Json) {
            ++all;
            return true;
        });
        check(full.has_value() && *full == document->size() && all == document->size(),
              "for_each walks every element of a valid array");
    }
}

void fuzz_utils_document(FuzzedDataProvider & provider)
{
    const std::string key = "t" + std::string(provider.ConsumeIntegralInRange<size_t>(0, 3), 'a');
    const std::string target_value = json_value(provider, 2);

    const size_t member_count = provider.ConsumeIntegralInRange<size_t>(1, 4);
    const size_t target_position = provider.ConsumeIntegralInRange<size_t>(0, member_count - 1);
    std::vector<std::string> members;
    for (size_t i = 0; i < member_count; ++i)
    {
        if (i == target_position)
            members.push_back(json_member(provider, key, target_value));
        else
            members.push_back(json_member(provider, "k" + std::to_string(i), json_value(provider, 2)));
    }

    std::vector<std::string> elements = {"{" + consume_spaces(provider) + json_list(provider, members) + "}"};
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
        elements.push_back(json_value(provider, 2));
    const std::string document = "[" + consume_spaces(provider) + json_list(provider, elements) + "]";

    const std::expected<Json, std::string> expected_target = Json::parse(target_value);
    check(expected_target.has_value(), "the generated value is valid json");
    const std::expected<Json, std::string> found = utils::find_first(document, key);
    check(found.has_value() && *found == *expected_target, "find_first returns the value of a key of the first object");
    const std::expected<Json, std::string> missing = utils::find_first(document, "absent");
    check(!missing.has_value() && missing.error() == "key 'absent' not found", "find_first reports an absent key");

    const size_t limit = provider.ConsumeIntegralInRange<size_t>(1, elements.size() + 1);
    std::vector<Json> seen;
    const std::expected<size_t, std::string> walked = utils::for_each(document, [&](Json element) {
        seen.push_back(std::move(element));
        return seen.size() < limit;
    });
    check(walked.has_value() && *walked == std::min(limit, elements.size()) && seen.size() == *walked,
          "for_each walks the elements up to the one that stops it");
    for (size_t i = 0; i < seen.size(); ++i)
    {
        const std::expected<Json, std::string> expected = Json::parse(elements[i]);
        check(expected.has_value() && seen[i] == *expected, "for_each delivers the elements in order");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_utils_document(provider);
    else
        fuzz_utils_raw(provider);
    return 0;
}
