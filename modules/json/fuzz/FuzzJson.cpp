#include <algorithm>
#include <array>
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

bool type_matches(const Json & json, Json::Type type)
{
    const std::array<bool, 8> predicates = {json.is_null(),
                                            json.is_bool(),
                                            json.is_number_integer(),
                                            json.is_number_unsigned(),
                                            json.is_number_float(),
                                            json.is_string(),
                                            json.is_array(),
                                            json.is_object()};
    for (size_t i = 0; i < predicates.size(); ++i)
    {
        if (predicates[i] != (static_cast<size_t>(type) == i))
            return false;
    }
    return json.type() == type && !json.is_discarded();
}

Json typed(Json json, Json::Type type)
{
    check(type_matches(json, type), "a built value has the type it was built with and no other");
    return json;
}

Json consume_json(FuzzedDataProvider & provider, int depth)
{
    switch (provider.ConsumeIntegralInRange<int>(0, depth > 0 ? 7 : 5))
    {
        case 0:
            return typed(Json(nullptr), Json::Type::Null);
        case 1:
            return typed(Json(provider.ConsumeBool()), Json::Type::Bool);
        case 2:
            return typed(Json(provider.ConsumeIntegral<int64_t>()), Json::Type::Int);
        case 3:
            return typed(Json((uint64_t(1) << 63) | provider.ConsumeIntegral<uint64_t>()), Json::Type::Uint);
        case 4:
            return typed(Json(provider.ConsumeFloatingPoint<double>()), Json::Type::Float);
        case 5:
            return typed(Json(consume_string(provider)), Json::Type::String);
        case 6:
        {
            Json::Array array;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
                array.push_back(consume_json(provider, depth - 1));
            return typed(Json(std::move(array)), Json::Type::Array);
        }
        default:
        {
            Json::Object object;
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
            {
                std::string key = consume_string(provider);
                object.emplace_back(std::move(key), consume_json(provider, depth - 1));
            }
            return typed(Json(std::move(object)), Json::Type::Object);
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

size_t expected_line_breaks(const Json & json)
{
    if (!json.is_array() && !json.is_object())
        return 0;
    size_t breaks = json.size() + 1;
    for (const Json & child : json)
        breaks += expected_line_breaks(child);
    return breaks;
}

void check_dump_layout(const Json & json, int indent)
{
    const std::string dumped = json.dump(indent);
    const size_t breaks = static_cast<size_t>(std::count(dumped.begin(), dumped.end(), '\n'));
    check(breaks == (indent < 0 ? 0 : expected_line_breaks(json)), "a dump breaks lines only with a positive indent");
    if (indent <= 0)
        return;

    size_t line_start = 0;
    while (line_start < dumped.size())
    {
        const size_t spaces = dumped.find_first_not_of(' ', line_start) - line_start;
        check(spaces % static_cast<size_t>(indent) == 0, "a line is indented by a multiple of the indent");
        const size_t line_end = dumped.find('\n', line_start);
        if (line_end == std::string::npos)
            break;
        line_start = line_end + 1;
    }
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
    check_dump_layout(*parsed, indent);
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
    walk(built);
    check_dump_layout(built, indent);

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
