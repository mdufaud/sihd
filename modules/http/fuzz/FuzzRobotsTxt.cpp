#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/http/navigator/RobotsTxt.hpp>
#include <sihd/util/str.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::http;
using sihd::util::str::to_upper_copy;

namespace
{
struct Rule
{
        bool allow;
        std::string path;
};

constexpr std::string_view path_chars = "ab/";
constexpr std::string_view eols[] = {"\n", "\r\n"};

bool expected_allowed(const std::vector<Rule> & rules, std::string_view path)
{
    const Rule *best = nullptr;
    for (const Rule & rule : rules)
    {
        if (rule.path.empty() || !path.starts_with(rule.path))
            continue;
        if (best == nullptr || rule.path.size() > best->path.size()
            || (rule.path.size() == best->path.size() && rule.allow))
            best = &rule;
    }
    return best == nullptr || best->allow;
}

std::string consume_eol(FuzzedDataProvider & provider)
{
    return std::string(eols[provider.ConsumeIntegralInRange<size_t>(0, std::size(eols) - 1)]);
}

std::string consume_keyword(FuzzedDataProvider & provider, std::string_view keyword)
{
    return provider.ConsumeBool() ? to_upper_copy(keyword) : std::string(keyword);
}

void fuzz_parse_raw(FuzzedDataProvider & provider)
{
    const std::string agent = provider.ConsumeRandomLengthString(16);
    const std::string path = provider.ConsumeRandomLengthString(32);
    const std::vector<char> content = provider.ConsumeRemainingBytes<char>();

    RobotsTxt once;
    once.parse(test::view_of(content));
    RobotsTxt twice;
    twice.parse(test::view_of(content));
    twice.parse(test::view_of(content));
    test::fuzz_check(once.is_allowed(agent, path) == twice.is_allowed(agent, path),
                     "parsing twice gives the same answers as parsing once");
    test::fuzz_check(once.crawl_delay(agent) == twice.crawl_delay(agent),
                     "parsing twice gives the same crawl delay as parsing once");

    once.clear();
    test::fuzz_check(once.is_allowed(agent, path) && !once.crawl_delay(agent).has_value(),
                     "a cleared robots.txt allows everything");
}

void fuzz_agent_groups(FuzzedDataProvider & provider)
{
    const std::string eol = consume_eol(provider);
    const std::string bot = "bot" + test::consume_word(provider, 1, 6);
    const std::string other = "x" + test::consume_word(provider, 1, 6);
    const std::string allowed_prefix = "/p" + test::consume_word(provider, 1, 5);
    const long delay = provider.ConsumeIntegralInRange<long>(-1000, 100000);
    const std::string suffix = "/" + test::consume_text(provider, path_chars, 0, 6);

    std::string robots = consume_keyword(provider, "User-agent") + ": " + bot + eol + "Disallow: /" + eol + eol;
    robots += consume_keyword(provider, "User-agent") + ": *" + eol + "Crawl-delay: " + std::to_string(delay) + eol
              + "Disallow: /" + eol + "Allow: " + allowed_prefix + eol;

    RobotsTxt robots_txt;
    robots_txt.parse(robots);
    test::fuzz_check(!robots_txt.is_allowed(to_upper_copy(bot), suffix),
                     "a group naming the agent disallows its paths");
    test::fuzz_check(!robots_txt.crawl_delay(to_upper_copy(bot)).has_value(),
                     "the group of the agent has no crawl delay");
    test::fuzz_check(!robots_txt.is_allowed(other, suffix), "the wildcard group disallows an unknown agent");
    test::fuzz_check(robots_txt.is_allowed(other, allowed_prefix + suffix), "the longest matching rule wins");
    test::fuzz_check(robots_txt.crawl_delay(other) == delay, "the wildcard group gives its crawl delay");
}

void fuzz_longest_match(FuzzedDataProvider & provider)
{
    const std::string eol = consume_eol(provider);
    std::vector<Rule> rules;
    std::string robots = consume_keyword(provider, "User-agent") + ": *" + eol;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 8); i > 0; --i)
    {
        Rule rule {provider.ConsumeBool(), std::string()};
        if (provider.ConsumeIntegralInRange<int>(0, 5) != 0)
            rule.path = "/" + test::consume_text(provider, path_chars, 0, 4);
        robots += consume_keyword(provider, rule.allow ? "Allow" : "Disallow") + (provider.ConsumeBool() ? ": " : ":")
                  + rule.path + (provider.ConsumeBool() ? " # note" : "") + eol;
        rules.push_back(std::move(rule));
    }

    RobotsTxt robots_txt;
    robots_txt.parse(robots);
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 6); i > 0; --i)
    {
        const std::string path = "/" + test::consume_text(provider, path_chars, 0, 6);
        test::fuzz_check(robots_txt.is_allowed("anybot", path) == expected_allowed(rules, path),
                         "the longest matching rule wins, allow on a tie");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_parse_raw(provider);
            break;
        case 1:
            fuzz_agent_groups(provider);
            break;
        default:
            fuzz_longest_match(provider);
            break;
    }
    return 0;
}
