#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/Environment.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::sys;

namespace
{
constexpr std::string_view name_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
constexpr std::string_view separators = " /.-:,;";
constexpr std::string_view unknown_reference = "${NOPE}";

using Entries = std::map<std::string, std::string>;

std::vector<std::string> consume_entries(FuzzedDataProvider & provider)
{
    std::string text = provider.ConsumeRemainingBytesAsString();
    std::erase(text, '\0');
    std::istringstream stream(text);
    std::vector<std::string> entries;
    for (std::string entry; std::getline(stream, entry);)
        entries.push_back(entry);
    return entries;
}

Entries expected_entries(const std::vector<std::string> & entries)
{
    Entries expected;
    for (const std::string & entry : entries)
    {
        const size_t equals = entry.find('=');
        if (equals != std::string::npos && equals > 0)
            expected[entry.substr(0, equals)] = entry.substr(equals + 1);
    }
    return expected;
}

void check_entries(const Environment & env, const Entries & expected)
{
    test::fuzz_check(env.entries() == expected, "an environment holds the loaded entries");
    test::fuzz_check(env.size() == expected.size(), "size counts the keys");
    test::fuzz_check(env.empty() == expected.empty(), "empty means no key");
    for (const auto & [key, value] : expected)
    {
        test::fuzz_check(env.get(key) == value, "get returns the loaded value");
        const std::string other = key + '\0';
        test::fuzz_check(env.get(other).has_value() == expected.contains(other), "get finds exactly the loaded keys");
    }
}

std::string consume_text(FuzzedDataProvider & provider, size_t max_length)
{
    std::string text = provider.ConsumeRandomLengthString(max_length);
    std::erase(text, '$');
    return text;
}

std::string consume_name(FuzzedDataProvider & provider)
{
    std::string name = "K";
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 8); i > 0; --i)
        name += name_chars[provider.ConsumeIntegralInRange<size_t>(0, name_chars.size() - 1)];
    return name;
}

void fuzz_load(FuzzedDataProvider & provider)
{
    const std::vector<std::string> entries = consume_entries(provider);
    std::vector<std::string_view> views(entries.begin(), entries.end());
    std::vector<const char *> raws;
    for (const std::string & entry : entries)
        raws.push_back(entry.c_str());
    const Entries expected = expected_entries(entries);

    Environment from_strings;
    from_strings.load(entries);
    check_entries(from_strings, expected);

    Environment from_views;
    from_views.load(views);
    check_entries(from_views, expected);

    Environment from_raws;
    from_raws.load(raws);
    check_entries(from_raws, expected);
}

void fuzz_operations(FuzzedDataProvider & provider)
{
    Environment env;
    Entries model;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 32); i > 0; --i)
    {
        const std::string key = provider.ConsumeBool()
                                    ? "K" + std::to_string(provider.ConsumeIntegralInRange<int>(0, 3))
                                    : provider.ConsumeRandomLengthString(6);
        switch (provider.ConsumeIntegralInRange<int>(0, 3))
        {
            case 0:
            {
                const std::string value = provider.ConsumeRandomLengthString(8);
                env.set(key, value);
                model[key] = value;
                test::fuzz_check(env.get(key) == value, "get returns what set stored");
                break;
            }
            case 1:
                test::fuzz_check(env.rm(key) == (model.erase(key) > 0), "rm tells whether the key was there");
                test::fuzz_check(!env.get(key).has_value(), "a removed key is gone");
                break;
            case 2:
            {
                const auto found = model.find(key);
                test::fuzz_check(
                    env.get(key) == (found == model.end() ? std::nullopt : std::optional<std::string>(found->second)),
                    "get follows the stored keys");
                break;
            }
            default:
                env.clear();
                model.clear();
                break;
        }
        check_entries(env, model);
    }
}

void fuzz_expand_unknown(FuzzedDataProvider & provider)
{
    const std::vector<char> text = provider.ConsumeRemainingBytes<char>();
    const std::string_view view(text.data(), text.size());
    test::fuzz_check(Environment().expand(view) == view, "nothing is expanded without variables");
}

void fuzz_expand_known(FuzzedDataProvider & provider)
{
    const std::string name = consume_name(provider);
    const std::string value = consume_text(provider, 16);
    Environment env;
    env.set(name, value);

    std::string text;
    std::string expected;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 8); i > 0; --i)
    {
        switch (provider.ConsumeIntegralInRange<int>(0, 3))
        {
            case 0:
            {
                const std::string literal = consume_text(provider, 8);
                text += literal;
                expected += literal;
                break;
            }
            case 1:
            {
                const char separator = separators[provider.ConsumeIntegralInRange<size_t>(0, separators.size() - 1)];
                text += '$' + name + separator;
                expected += value + separator;
                break;
            }
            case 2:
                text += "${" + name + '}';
                expected += value;
                break;
            default:
                text += unknown_reference;
                expected += unknown_reference;
                break;
        }
    }
    test::fuzz_check(env.expand(text) == expected, "known references are replaced and unknown ones are kept");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_load(provider);
            break;
        case 1:
            fuzz_operations(provider);
            break;
        case 2:
            fuzz_expand_unknown(provider);
            break;
        default:
            fuzz_expand_known(provider);
            break;
    }
    return 0;
}
