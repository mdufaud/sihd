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

struct Generated
{
        std::string pattern;
        std::string path;
        std::string wrong_literal_path;
        std::string short_path;
        Params params;
        size_t literals = 0;
        bool catch_all = false;
};

std::string make_literal(FuzzedDataProvider & provider)
{
    const std::string word = test::consume_word(provider, 1, 6);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 1:
            return "{" + word;
        case 2:
            return word + "}";
        case 3:
            return "{}";
        default:
            return word;
    }
}

std::string make_name(FuzzedDataProvider & provider, size_t index)
{
    return std::string(1, static_cast<char>('a' + index)) + test::consume_word(provider, 0, 3);
}

Generated generate_route(FuzzedDataProvider & provider)
{
    Generated route;
    std::vector<std::string> segments;
    size_t wrong_index = std::string::npos;
    bool dots_used = false;
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 5);
    for (size_t i = 0; i < count && !route.catch_all; ++i)
    {
        const std::string value = test::consume_word(provider, 1, 6);
        const std::string name = make_name(provider, i);
        switch (provider.ConsumeIntegralInRange<int>(0, 4))
        {
            case 2:
                route.pattern += "/{" + name + "}";
                segments.push_back(value);
                route.params[name] = value;
                break;
            case 3:
            {
                std::string rest = value;
                for (size_t more = provider.ConsumeIntegralInRange<size_t>(0, 2); more > 0; --more)
                    rest += "/" + test::consume_word(provider, 1, 4);
                route.pattern += "/{" + name + "...}";
                segments.push_back(rest);
                route.params[name] = rest;
                route.catch_all = true;
                break;
            }
            case 4:
                if (dots_used)
                    break;
                // three dots is too short to be a catch-all name: a plain parameter called "..."
                route.pattern += "/{...}";
                segments.push_back(value);
                route.params["..."] = value;
                dots_used = true;
                break;
            default:
            {
                const std::string literal = make_literal(provider);
                route.pattern += "/" + literal;
                segments.push_back(literal);
                wrong_index = segments.size() - 1;
                ++route.literals;
                break;
            }
        }
    }

    for (size_t i = 0; i < segments.size(); ++i)
    {
        route.path += "/" + segments[i];
        route.wrong_literal_path += "/" + segments[i] + (i == wrong_index ? "x" : "");
        if (i + 1 < segments.size())
            route.short_path += "/" + segments[i];
    }
    return route;
}

void fuzz_route_match(FuzzedDataProvider & provider)
{
    const Generated generated = generate_route(provider);

    const Route route(generated.pattern, {});
    const RouteMatch match = route.match(generated.path);
    test::fuzz_check(match.matched, "a path built from the pattern matches it");
    test::fuzz_check(match.params == generated.params, "the parameters are captured");
    test::fuzz_check(route.pattern() == generated.pattern, "the pattern is kept");
    test::fuzz_check(route.specificity() == generated.literals, "the specificity counts the literal segments");
    test::fuzz_check(route.is_parameterized() == !generated.params.empty(),
                     "a route with a parameter is parameterized");
    test::fuzz_check(route.has_catch_all() == generated.catch_all, "a route knows its catch-all");

    if (generated.literals > 0)
        test::fuzz_check(!route.match(generated.wrong_literal_path).matched, "another literal segment does not match");
    if (!generated.catch_all)
        test::fuzz_check(!route.match(generated.path + "/extra").matched,
                         "a longer path does not match without a catch-all");
    test::fuzz_check(!route.match(generated.short_path).matched, "a path missing its last segment does not match");
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
        {"/t/{a}/{b}", HttpRequest::Get},
        {"/t/{a}/" + literal, HttpRequest::Get},
        {"/t/{r...}", HttpRequest::Get},
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

    const auto specific_found = table.find(HttpRequest::Get, messy("/t/" + other + "/" + literal));
    test::fuzz_check(specific_found.has_value() && specific_found->match.params == Params {{"a", other}},
                     "a parameter route with more literals wins over a less specific one");

    const auto two_params_found = table.find(HttpRequest::Get, messy("/t/" + literal + "/" + other));
    test::fuzz_check(
        two_params_found.has_value() && two_params_found->match.params == Params {{"a", literal}, {"b", other}},
        "a parameter route wins over a catch-all");

    const auto deep_found = table.find(HttpRequest::Get, messy("/t/" + literal + "/" + other + "/" + literal));
    test::fuzz_check(
        deep_found.has_value() && deep_found->match.params == Params {{"r", literal + "/" + other + "/" + literal}},
        "a catch-all answers when no other route has the path length");

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
