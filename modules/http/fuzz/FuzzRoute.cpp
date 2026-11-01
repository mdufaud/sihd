#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sihd/http/Route.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::http;

namespace
{
using Params = std::unordered_map<std::string, std::string>;

std::string double_slashes(std::string_view path)
{
    std::string result;
    for (const char c : path)
        result += c == '/' ? "//" : std::string(1, c);
    return result;
}

std::string messy(std::string_view path)
{
    return "/" + double_slashes(path) + "/";
}

void fuzz_normalize_path(FuzzedDataProvider & provider)
{
    const std::vector<char> raw_bytes = provider.ConsumeRemainingBytes<char>();
    const std::string_view raw = test::view_of(raw_bytes);

    const std::string normalized = normalize_path(raw);
    test::fuzz_check(normalized.find("//") == std::string::npos, "a normalized path has no double slash");
    test::fuzz_check(normalized.empty() || normalized.back() != '/', "a normalized path has no trailing slash");
    test::fuzz_check(normalize_path(normalized) == normalized, "normalizing is idempotent");
    test::fuzz_check(normalize_path(std::string(raw) + "/") == normalized, "a trailing slash is stripped");
    test::fuzz_check(normalize_path(double_slashes(raw)) == normalized, "double slashes collapse");
}

void fuzz_route_raw(FuzzedDataProvider & provider)
{
    const std::string pattern = provider.ConsumeRandomLengthString(48);
    const std::vector<char> path_bytes = provider.ConsumeRemainingBytes<char>();
    const std::string_view path = test::view_of(path_bytes);

    const Route route(pattern, {});
    const RouteMatch match = route.match(path);
    test::fuzz_check(match.matched || match.params.empty(), "an unmatched route captures nothing");

    RouteTable table;
    table.add(pattern, {}, HttpRequest::Get);
    (void)table.find(HttpRequest::Get, path);
    (void)table.allowed_methods(path);
}

void fuzz_route_match(FuzzedDataProvider & provider)
{
    std::string pattern;
    std::string path;
    std::string wrong_path;
    Params params;
    size_t literals = 0;
    bool catch_all = false;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 5); i > 0 && !catch_all; --i)
    {
        const std::string value = test::consume_word(provider, 1, 6);
        const std::string name = "p" + std::to_string(params.size());
        switch (provider.ConsumeIntegralInRange<int>(0, 3))
        {
            case 2:
                pattern += "/{" + name + "}";
                path += "/" + value;
                wrong_path += "/" + value;
                params[name] = value;
                break;
            case 3:
            {
                const std::string rest = value + "/" + test::consume_word(provider, 1, 6);
                pattern += "/{" + name + "...}";
                path += "/" + rest;
                wrong_path += "/" + rest;
                params[name] = rest;
                catch_all = true;
                break;
            }
            default:
                pattern += "/" + value;
                path += "/" + value;
                wrong_path += "/" + value + "x";
                ++literals;
                break;
        }
    }

    const Route route(pattern, {});
    const RouteMatch match = route.match(path);
    test::fuzz_check(match.matched, "a path built from the pattern matches it");
    test::fuzz_check(match.params == params, "the parameters are captured");
    test::fuzz_check(route.pattern() == pattern, "the pattern is kept");
    test::fuzz_check(route.specificity() == literals, "the specificity counts the literal segments");
    test::fuzz_check(route.is_parameterized() == !params.empty(), "a route with a parameter is parameterized");
    test::fuzz_check(route.has_catch_all() == catch_all, "a route knows its catch-all");
    if (literals > 0)
        test::fuzz_check(!route.match(wrong_path).matched, "another literal segment does not match");
    if (!catch_all)
        test::fuzz_check(!route.match(path + "/extra").matched, "a longer path does not match without a catch-all");
}

void fuzz_route_table(FuzzedDataProvider & provider)
{
    const std::string literal = test::consume_word(provider, 1, 6);
    const std::string other = literal + test::consume_word(provider, 1, 3);
    const std::string literal_path = "/s/" + literal;

    std::vector<std::pair<std::string, HttpRequest::RequestType>> routes = {
        {"/s/{rest...}", HttpRequest::Get},
        {"/s/{p}", HttpRequest::Get},
        {literal_path, HttpRequest::Get},
        {literal_path, HttpRequest::Post},
    };
    std::ranges::rotate(routes, routes.begin() + provider.ConsumeIntegralInRange<std::ptrdiff_t>(0, routes.size() - 1));

    RouteTable table;
    for (const auto & [pattern, method] : routes)
        table.add(pattern, {}, method);

    const auto literal_found = table.find(HttpRequest::Get, messy(literal_path));
    test::fuzz_check(literal_found.has_value() && literal_found->match.params.empty(),
                     "a literal route wins over parameterized ones");

    const auto param_found = table.find(HttpRequest::Get, messy("/s/" + other));
    test::fuzz_check(param_found.has_value() && param_found->match.params == Params {{"p", other}},
                     "a parameter route wins over a catch-all");

    const auto catch_all_found = table.find(HttpRequest::Get, messy("/s/" + other + "/" + literal));
    test::fuzz_check(
        catch_all_found.has_value() && catch_all_found->match.params == Params {{"rest", other + "/" + literal}},
        "a catch-all route takes the rest of the path");

    test::fuzz_check(table.find(HttpRequest::Post, literal_path).has_value(), "a route is found for its method");
    test::fuzz_check(!table.find(HttpRequest::Post, "/s/" + other).has_value(),
                     "a route is not found for another method");
    test::fuzz_check(!table.find(HttpRequest::Put, literal_path).has_value(),
                     "no route answers an unregistered method");
    test::fuzz_check(!table.find(HttpRequest::Get, "/s").has_value(), "a shorter path matches no route");

    const auto literal_methods = table.allowed_methods(messy(literal_path));
    test::fuzz_check(literal_methods.size() == 2 && std::ranges::count(literal_methods, HttpRequest::Get) == 1
                         && std::ranges::count(literal_methods, HttpRequest::Post) == 1,
                     "the methods of a path are the ones of the routes matching it");
    const auto other_methods = table.allowed_methods("/s/" + other);
    test::fuzz_check(other_methods.size() == 1 && other_methods.front() == HttpRequest::Get,
                     "only the parameter route answers another path");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_normalize_path(provider);
            break;
        case 1:
            fuzz_route_raw(provider);
            break;
        case 2:
            fuzz_route_match(provider);
            break;
        default:
            fuzz_route_table(provider);
            break;
    }
    return 0;
}
