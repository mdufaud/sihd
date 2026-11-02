#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/http/Multipart.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using sihd::http::Multipart;

namespace
{
constexpr std::string_view quoted_boundary_chars = "ab Z09'()+_,-./:=?";
constexpr std::string_view padding_chars = " \t";
constexpr std::string_view name_chars = "abcXYZ019_-.";
constexpr std::string_view filename_chars = "abcXYZ019_-. ";
constexpr std::array<std::string_view, 3> media_types = {"text/plain", "application/octet-stream", "image/png"};

struct ExpectedPart
{
        std::string name;
        std::string filename;
        std::string content_type;
        std::string data;
};

struct Built
{
        std::string content_type;
        std::string boundary;
        std::string body;
        size_t closing_end = 0;
        std::vector<ExpectedPart> parts;
};

std::string flip_case(std::string_view text, bool upper)
{
    std::string result(text);
    for (char & c : result)
        c = static_cast<char>(upper ? std::toupper(static_cast<unsigned char>(c))
                                    : std::tolower(static_cast<unsigned char>(c)));
    return result;
}

std::string make_boundary(FuzzedDataProvider & provider, bool quoted)
{
    if (!quoted)
        return test::consume_word(provider, 1, 20);
    return "b" + test::consume_text(provider, quoted_boundary_chars, 0, 8) + "b";
}

std::string make_content_type(FuzzedDataProvider & provider, const std::string & boundary, bool quoted)
{
    std::string type = flip_case("multipart/form-data", provider.ConsumeBool());
    std::string content_type = test::consume_text(provider, padding_chars, 0, 2) + type
                               + test::consume_text(provider, padding_chars, 0, 2);
    if (provider.ConsumeBool())
        content_type += "; charset=utf-8";
    content_type += ";" + test::consume_text(provider, padding_chars, 0, 2)
                    + flip_case("boundary", provider.ConsumeBool()) + "=";
    content_type += quoted ? "\"" + boundary + "\"" : boundary;
    if (provider.ConsumeBool())
        content_type += "; x=y";
    return content_type;
}

// data may hold boundary-like bytes as long as they do not end a line like a delimiter
std::string make_data(FuzzedDataProvider & provider, const std::string & boundary)
{
    std::string data = provider.ConsumeRandomLengthString(24);
    const std::string delimiter = "--" + boundary;
    if (data.find(delimiter) != std::string::npos)
        data = "x";
    if (provider.ConsumeBool())
        data += "\r\n" + delimiter + test::consume_word(provider, 1, 3) + "\r\n";
    return data;
}

ExpectedPart make_part(FuzzedDataProvider & provider, const std::string & boundary)
{
    ExpectedPart part;
    part.name = test::consume_text(provider, name_chars, 1, 10);
    if (provider.ConsumeBool())
        part.filename = test::consume_text(provider, filename_chars, 1, 10) + ".bin";
    if (provider.ConsumeBool())
        part.content_type = media_types[provider.ConsumeIntegralInRange<size_t>(0, media_types.size() - 1)];
    part.data = make_data(provider, boundary);
    return part;
}

std::string part_headers(FuzzedDataProvider & provider, const ExpectedPart & part)
{
    std::string disposition = flip_case("content-disposition", provider.ConsumeBool()) + ": form-data; name=\""
                              + part.name + "\"";
    if (!part.filename.empty())
        disposition += "; filename=\"" + part.filename + "\"";
    disposition += "\r\n";

    std::string content_type;
    if (!part.content_type.empty())
        content_type = flip_case("content-type", provider.ConsumeBool()) + ": " + part.content_type + "\r\n";

    const std::string extra = provider.ConsumeBool() ? "X-Extra: " + test::consume_word(provider, 1, 6) + "\r\n" : "";
    if (provider.ConsumeBool())
        return content_type + disposition + extra;
    return disposition + extra + content_type;
}

Built build_multipart(FuzzedDataProvider & provider)
{
    Built built;
    const bool quoted = provider.ConsumeBool();
    built.boundary = make_boundary(provider, quoted);
    built.content_type = make_content_type(provider, built.boundary, quoted);

    const std::string delimiter = "--" + built.boundary;
    if (provider.ConsumeBool())
        built.body = test::consume_word(provider, 1, 6) + "\r\n";
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 5); i > 0; --i)
    {
        ExpectedPart part = make_part(provider, built.boundary);
        built.body += delimiter + test::consume_text(provider, padding_chars, 0, 2) + "\r\n";
        built.body += part_headers(provider, part) + "\r\n" + part.data + "\r\n";
        built.parts.push_back(std::move(part));
    }
    built.body += delimiter + "--";
    built.closing_end = built.body.size();
    if (provider.ConsumeBool())
        built.body += "\r\n" + provider.ConsumeRandomLengthString(16);
    return built;
}

bool same_part(const Multipart::Part & got, const ExpectedPart & expected)
{
    return got.name == expected.name && got.filename == expected.filename && got.content_type == expected.content_type
           && got.data == expected.data && got.path.empty() && got.is_file() == !expected.filename.empty();
}

void check_lookups(const Multipart & multipart, const std::vector<ExpectedPart> & expected)
{
    for (const ExpectedPart & wanted : expected)
    {
        const auto first = std::ranges::find(expected, wanted.name, &ExpectedPart::name);
        test::fuzz_check(multipart.value(wanted.name) == std::optional<std::string_view>(first->data),
                         "value returns the data of the first part with that name");

        const auto first_file = std::ranges::find_if(expected, [&](const ExpectedPart & part) {
            return part.name == wanted.name && !part.filename.empty();
        });
        const Multipart::Part *file = multipart.file(wanted.name);
        test::fuzz_check((file == nullptr) == (first_file == expected.end()),
                         "file finds a part only when one with that name has a filename");
        if (file != nullptr)
            test::fuzz_check(file->data == first_file->data, "file returns the first file part with that name");
    }
    test::fuzz_check(!multipart.value("absent").has_value() && multipart.file("absent") == nullptr,
                     "an unknown name is not found");
}

void check_parsed(const Built & built)
{
    const test::ExactText body(built.body);
    const auto parsed = Multipart::parse(body, built.content_type);
    test::fuzz_check(parsed.has_value(), "a multipart body built from known parts parses");
    test::fuzz_check(parsed->parts().size() == built.parts.size(), "every part is parsed, none is invented");
    test::fuzz_check(parsed->empty() == built.parts.empty(), "empty follows the part count");
    for (size_t i = 0; i < built.parts.size() && i < parsed->parts().size(); ++i)
        test::fuzz_check(same_part(parsed->parts()[i], built.parts[i]),
                         "a part keeps its name, filename, content type and data");
    check_lookups(*parsed, built.parts);
}

void check_content_type(const Built & built)
{
    test::fuzz_check(Multipart::is_content_type(built.content_type), "a multipart/form-data type is recognized");
    test::fuzz_check(Multipart::boundary(built.content_type) == std::optional<std::string>(built.boundary),
                     "the boundary is found back, quoted or not");

    const std::string other = "text/plain; boundary=" + built.boundary;
    test::fuzz_check(!Multipart::is_content_type(other) && !Multipart::boundary(other).has_value(),
                     "another media type has no multipart boundary");
    test::fuzz_check(!Multipart::parse(built.body, other).has_value(), "a body is not parsed with another media type");

    const std::string no_boundary = "multipart/form-data; charset=utf-8";
    test::fuzz_check(Multipart::is_content_type(no_boundary) && !Multipart::boundary(no_boundary).has_value(),
                     "a multipart type without boundary is still multipart but has none");
    test::fuzz_check(!Multipart::parse(built.body, no_boundary).has_value(), "a body is not parsed without boundary");
}

void check_truncated(FuzzedDataProvider & provider, const Built & built)
{
    const size_t cut = provider.ConsumeIntegralInRange<size_t>(0, built.closing_end - 1);
    const test::ExactText cut_body(std::string_view(built.body).substr(0, cut));
    test::fuzz_check(!Multipart::parse(cut_body, built.content_type).has_value(),
                     "a body cut before its closing delimiter is refused");

    const test::ExactText exact_body(std::string_view(built.body).substr(0, built.closing_end));
    const auto parsed = Multipart::parse(exact_body, built.content_type);
    test::fuzz_check(parsed.has_value() && parsed->parts().size() == built.parts.size(),
                     "a body ending exactly with its closing delimiter parses");
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    const Built built = build_multipart(provider);

    check_content_type(built);
    check_parsed(built);
    check_truncated(provider, built);
}

bool is_valid_identifier(std::string_view text)
{
    return text.find_first_of("\r\n\0\"", 0, 4) == std::string_view::npos;
}

void fuzz_builder(FuzzedDataProvider & provider)
{
    Multipart multipart;
    std::vector<ExpectedPart> expected;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 6); i > 0; --i)
    {
        const std::string name = provider.ConsumeRandomLengthString(8);
        const std::string filename = provider.ConsumeRandomLengthString(8);
        const std::string value = provider.ConsumeRandomLengthString(8);
        const bool is_file = provider.ConsumeBool();
        const size_t before = multipart.parts().size();

        const bool accepted = is_valid_identifier(name)
                              && (!is_file || filename.empty() || is_valid_identifier(filename));
        if (is_file)
            multipart.add_file(name, value, filename);
        else
            multipart.add_field(name, value);

        test::fuzz_check(multipart.parts().size() == before + (accepted ? 1 : 0),
                         "a name or filename with CR, LF, NUL or a quote is refused");
        if (accepted)
            expected.push_back({name, is_file ? filename : std::string(), {}, is_file ? std::string() : value});
    }

    test::fuzz_check(multipart.parts().size() == expected.size(), "only accepted parts are kept");
    for (size_t i = 0; i < expected.size(); ++i)
        test::fuzz_check(multipart.parts()[i].name == expected[i].name
                             && multipart.parts()[i].filename == expected[i].filename
                             && multipart.parts()[i].data == expected[i].data,
                         "parts keep their insertion order and content");
    multipart.clear();
    test::fuzz_check(multipart.empty(), "clear removes every part");
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const std::string content_type = provider.ConsumeRandomLengthString(80);
    const test::ExactText body(provider.ConsumeRemainingBytesAsString());

    const bool is_multipart = Multipart::is_content_type(content_type);
    const auto boundary = Multipart::boundary(content_type);
    test::fuzz_check(!boundary.has_value() || is_multipart, "only a multipart type has a boundary");
    test::fuzz_check(!boundary.has_value() || !boundary->empty(), "a found boundary is never empty");

    const auto parsed = Multipart::parse(body, content_type);
    test::fuzz_check(boundary.has_value() || !parsed.has_value(), "a body without boundary is never parsed");
    if (parsed.has_value())
        test::fuzz_check(body.view().find("--" + *boundary) != std::string_view::npos,
                         "a parsed body contains its delimiter");
    (void)Multipart::parse(body, "multipart/form-data; boundary=b");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_raw(provider);
            break;
        case 1:
            fuzz_generated(provider);
            break;
        default:
            fuzz_builder(provider);
            break;
    }
    return 0;
}
