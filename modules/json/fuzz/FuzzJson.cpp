#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/json/Json.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::json;

namespace
{
constexpr int max_depth = 3;
constexpr std::string_view string_pieces[] = {
    "a",
    "Z",
    "0",
    " ",
    "\"",
    "\\",
    "/",
    "\n",
    "\t",
    "\x01",
    "\x1f",
    "\x7f",
    "\xc3\xa9",
    "\xe2\x82\xac",
    "\xf0\x9f\x98\x80",
    std::string_view("\0", 1),
};

void check(bool ok, const char *property)
{
    if (ok)
        return;
    std::fprintf(stderr, "fuzz property violated: %s\n", property);
    std::abort();
}

std::string consume_string(FuzzedDataProvider & provider)
{
    std::string text;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 6); i > 0; --i)
        text += string_pieces[provider.ConsumeIntegralInRange<size_t>(0, std::size(string_pieces) - 1)];
    return text;
}

Json consume_json(FuzzedDataProvider & provider, int depth)
{
    switch (provider.ConsumeIntegralInRange<int>(0, depth > 0 ? 7 : 5))
    {
        case 0:
            return Json(nullptr);
        case 1:
            return Json(provider.ConsumeBool());
        case 2:
            return Json(provider.ConsumeIntegral<int64_t>());
        case 3:
            return Json((uint64_t(1) << 63) | provider.ConsumeIntegral<uint64_t>());
        case 4:
            return Json(provider.ConsumeFloatingPoint<double>());
        case 5:
            return Json(consume_string(provider));
        case 6:
        {
            Json::Array array;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
                array.push_back(consume_json(provider, depth - 1));
            return Json(std::move(array));
        }
        default:
        {
            Json::Object object;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
            {
                std::string key = consume_string(provider);
                object.emplace_back(std::move(key), consume_json(provider, depth - 1));
            }
            return Json(std::move(object));
        }
    }
}

void walk(const Json & json)
{
    const int types = json.is_null() + json.is_bool() + json.is_number() + json.is_string() + json.is_array()
                      + json.is_object();
    check(types == 1, "a node has exactly one type");
    if (json.is_array())
    {
        size_t index = 0;
        for (const Json & child : json)
        {
            check(child == json[index], "indexing an array finds the iterated element");
            walk(child);
            ++index;
        }
        check(index == json.size(), "an array iteration visits size() elements");
    }
    else if (json.is_object())
    {
        size_t count = 0;
        for (const Json & child : json)
        {
            walk(child);
            ++count;
        }
        check(count == json.size(), "an object iteration visits size() members");
    }
    else if (json.is_string())
        (void)json.get<std::string>();
    else if (json.is_bool())
        (void)json.get<bool>();
    else if (json.is_number())
        (void)json.get<double>();
}

void fuzz_parse_raw(FuzzedDataProvider & provider)
{
    const int indent = provider.ConsumeIntegralInRange<int>(0, 4);
    const std::vector<char> text = provider.ConsumeRemainingBytes<char>();
    const std::string_view input(text.data(), text.size());

    const std::expected<Json, std::string> parsed = Json::parse(input);
    if (!parsed.has_value())
    {
        check(!parsed.error().empty(), "a parse failure explains itself");
        return;
    }

    walk(*parsed);
    check(Json(*parsed) == *parsed, "a copy equals its source");

    const std::expected<Json, std::string> compact = Json::parse(parsed->dump());
    check(compact.has_value() && *compact == *parsed, "dump then parse gives the same document");
    const std::expected<Json, std::string> pretty = Json::parse(parsed->dump(indent));
    check(pretty.has_value() && *pretty == *parsed, "dump with an indent then parse gives the same document");
    const std::expected<Json, std::string> padded = Json::parse(std::string(input) + " \r\n\t");
    check(padded.has_value() && *padded == *parsed, "whitespace after the document changes nothing");
}

void fuzz_built_round_trip(FuzzedDataProvider & provider)
{
    const int indent = provider.ConsumeIntegralInRange<int>(-1, 8);
    const Json built = consume_json(provider, max_depth);

    const std::expected<Json, std::string> parsed = Json::parse(built.dump(indent));
    check(parsed.has_value(), "a dumped document parses");
    check(*parsed == built, "dump then parse gives the same document");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_built_round_trip(provider);
    else
        fuzz_parse_raw(provider);
    return 0;
}
