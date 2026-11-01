#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/StrConfiguration.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr size_t max_text_size = 1024;

using Entries = std::map<std::string, std::vector<std::string>>;

Entries parse_reference(std::string_view text)
{
    Entries entries;
    size_t begin = 0;
    while (begin <= text.size())
    {
        const size_t end = std::min(text.find(';', begin), text.size());
        const std::string_view pair = text.substr(begin, end - begin);
        const size_t equal = pair.find('=');
        if (equal != std::string_view::npos && equal > 0)
            entries[std::string(pair.substr(0, equal))].emplace_back(pair.substr(equal + 1));
        begin = end + 1;
    }
    return entries;
}

std::string absent_key(const Entries & entries)
{
    std::string key = "absent";
    while (entries.contains(key))
        key += '#';
    return key;
}

void check_entries(const StrConfiguration & conf, const Entries & entries)
{
    test::fuzz_check(conf.size() == entries.size(), "one entry per distinct key");
    test::fuzz_check(conf.empty() == entries.empty(), "empty means no key");
    for (const auto & [key, values] : entries)
    {
        const std::optional<std::string> found = conf.find(key);
        test::fuzz_check(found.has_value(), "find reports every key");
        test::fuzz_check(std::ranges::find(values, *found) != values.end(), "a key holds one of its values");
        test::fuzz_check(conf.has(key), "has agrees with find");
        test::fuzz_check(conf[key] == *found, "operator[] agrees with find");
        test::fuzz_check(conf.get(key) == *found, "get agrees with find");
    }

    const std::string missing = absent_key(entries);
    test::fuzz_check(!conf.has(missing) && !conf.find(missing).has_value() && conf[missing].empty(),
                     "a missing key is not found");
    bool threw = false;
    try
    {
        (void)conf.get(missing);
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    test::fuzz_check(threw, "get throws on a missing key");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const test::ExactText text(provider.ConsumeBytesAsString(max_text_size));

    const StrConfiguration conf(text);
    check_entries(conf, parse_reference(text));

    const std::string dumped = conf.str();
    const Entries dumped_entries = parse_reference(dumped);
    test::fuzz_check(dumped_entries.size() == conf.size(), "str reports every key");
    for (const auto & [key, values] : dumped_entries)
        test::fuzz_check(values.size() == 1 && conf.find(key) == values.front(), "str reports each key once");
    check_entries(StrConfiguration(dumped), dumped_entries);
    return 0;
}
