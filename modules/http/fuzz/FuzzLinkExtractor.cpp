#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <sihd/http/navigator/LinkExtractor.hpp>
#include <sihd/util/str.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::http;
using sihd::util::str::to_upper_copy;

namespace
{
struct LinkKind
{
        std::string_view tag;
        std::string_view url_attr;
};

constexpr LinkKind link_kinds[] = {{"a", "href"}, {"img", "src"}, {"link", "href"}, {"script", "src"}};
constexpr std::string_view base_url = "http://base.example/dir/";
constexpr std::string_view base_origin = "http://base.example";
constexpr std::string_view text_chars = "abc XYZ\n019";
constexpr std::string_view decoys[] = {"<abbr href=\"/nope\">", "<div id=x>", "<p>", "</div>", "<!-- note -->"};

auto link_key(const ExtractedLink & link)
{
    return std::tie(link.tag, link.url, link.text, link.rel);
}

bool by_key(const ExtractedLink & a, const ExtractedLink & b)
{
    return link_key(a) < link_key(b);
}

bool same_key(const ExtractedLink & a, const ExtractedLink & b)
{
    return link_key(a) == link_key(b);
}

std::string quote(FuzzedDataProvider & provider, const std::string & value)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return "\"" + value + "\"";
        case 1:
            return "'" + value + "'";
        default:
            return value;
    }
}

std::string cased(bool upper, std::string_view name)
{
    return upper ? to_upper_copy(name) : std::string(name);
}

void fuzz_extract_raw(FuzzedDataProvider & provider)
{
    const std::string base = provider.ConsumeRandomLengthString(64);
    const std::vector<char> html = provider.ConsumeRemainingBytes<char>();

    const std::vector<ExtractedLink> links = extract_links(test::view_of(html), base);
    test::fuzz_check(links.size() <= static_cast<size_t>(std::ranges::count(html, '<')),
                     "every link comes from its own tag");
    for (const ExtractedLink & link : links)
    {
        test::fuzz_check(std::ranges::any_of(link_kinds, [&](const LinkKind & kind) { return kind.tag == link.tag; }),
                         "a link comes from an a, img, link or script tag");
        test::fuzz_check(!link.url.empty(), "a link has an url");
    }
}

void fuzz_extract_structured(FuzzedDataProvider & provider)
{
    std::string html;
    std::vector<ExtractedLink> expected;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 6); i > 0; --i)
    {
        html += test::consume_text(provider, text_chars, 0, 4);
        if (provider.ConsumeBool())
            html += decoys[provider.ConsumeIntegralInRange<size_t>(0, std::size(decoys) - 1)];

        const LinkKind & kind = link_kinds[provider.ConsumeIntegralInRange<size_t>(0, std::size(link_kinds) - 1)];
        const bool upper = provider.ConsumeBool();
        const std::string path = test::consume_word(provider, 1, 8);
        const bool relative = provider.ConsumeBool();
        const std::string href = relative ? "/" + path
                                          : "http://h" + test::consume_word(provider, 1, 6) + ".example/" + path;

        ExtractedLink link;
        link.tag = kind.tag;
        link.url = relative ? std::string(base_origin) + href : href;
        if (kind.tag == "a" || kind.tag == "img")
            link.text = test::consume_text(provider, text_chars, 0, 6);
        if (provider.ConsumeBool())
            link.rel = test::consume_word(provider, 1, 6);

        html += "<" + cased(upper, kind.tag) + " " + cased(upper, kind.url_attr) + "=" + quote(provider, href);
        if (!link.rel.empty())
            html += " " + cased(upper, "rel") + "=" + quote(provider, link.rel);
        if (kind.tag == "img")
            html += " " + cased(upper, "alt") + "=\"" + link.text + "\"";
        html += ">";
        if (kind.tag == "a")
            html += link.text + "</a>";
        else if (kind.tag == "script")
            html += "</script>";
        expected.push_back(std::move(link));
    }

    std::vector<ExtractedLink> links = extract_links(html, std::string(base_url));
    std::ranges::sort(links, by_key);
    std::ranges::sort(expected, by_key);
    test::fuzz_check(std::ranges::equal(links, expected, same_key), "the links of a well formed page are all found");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_extract_structured(provider);
    else
        fuzz_extract_raw(provider);
    return 0;
}
