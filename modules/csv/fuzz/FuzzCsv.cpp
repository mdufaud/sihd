#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/csv/utils.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::csv;

namespace
{
constexpr std::string_view plain_chars = "abcdefghijklmnopqrstuvwxyz0123456789.-_";
constexpr std::string_view cell_chars = "ab ,;|\t\n#\"'";
constexpr std::string_view delimiters = ",;|\t";
constexpr std::string_view comment_chars = "#;%";

std::string consume_text(FuzzedDataProvider & provider, std::string_view alphabet, size_t min_length, size_t max_length)
{
    std::string text(provider.ConsumeIntegralInRange<size_t>(min_length, max_length), 'a');
    for (char & c : text)
        c = alphabet[provider.ConsumeIntegralInRange<size_t>(0, alphabet.size() - 1)];
    return text;
}

char consume_char(FuzzedDataProvider & provider, std::string_view alphabet)
{
    return alphabet[provider.ConsumeIntegralInRange<size_t>(0, alphabet.size() - 1)];
}

void fuzz_parse_raw(FuzzedDataProvider & provider)
{
    const int delimiter = provider.ConsumeIntegral<uint8_t>();
    const char comment = provider.ConsumeIntegral<char>();
    const bool remove_header = provider.ConsumeBool();
    const std::vector<char> content = provider.ConsumeRemainingBytes<char>();
    const std::string_view input(content.data(), content.size());

    const utils::CsvData rows = utils::csv_from_string(input, remove_header, delimiter, comment);
    test::fuzz_check(rows.size() <= static_cast<size_t>(std::ranges::count(input, '\n')) + 1,
                     "a row comes from its own line");
}

void fuzz_parse_rows(FuzzedDataProvider & provider)
{
    const char delimiter = consume_char(provider, delimiters);
    const char comment = consume_char(provider, comment_chars);
    const bool remove_header = provider.ConsumeBool();

    utils::CsvData rows;
    std::string content;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 5); i > 0; --i)
    {
        utils::CsvLine row {consume_text(provider, plain_chars, 1, 6)};
        for (size_t j = provider.ConsumeIntegralInRange<size_t>(0, 4); j > 0; --j)
            row.push_back(consume_text(provider, plain_chars, 0, 6));
        for (size_t j = 0; j < row.size(); ++j)
            content += (j > 0 ? std::string(1, delimiter) : std::string()) + row[j];
        content += "\n";
        if (provider.ConsumeBool())
            content += std::string(1, comment) + consume_text(provider, plain_chars, 0, 6) + "\n";
        rows.push_back(std::move(row));
    }
    if (provider.ConsumeBool())
        content.pop_back();

    const utils::CsvData expected(rows.begin() + static_cast<std::ptrdiff_t>(remove_header), rows.end());
    test::fuzz_check(utils::csv_from_string(content, remove_header, delimiter, comment) == expected,
                     "plain rows are parsed back, comment lines skipped, the header removed on demand");
}

void fuzz_escape_str(FuzzedDataProvider & provider)
{
    const std::string cell = provider.ConsumeRandomLengthString(64);

    std::string expected = "\"";
    for (const char c : cell)
    {
        if (c == '"')
            expected += '"';
        expected += c;
    }
    expected += '"';
    test::fuzz_check(utils::escape_str(cell) == expected, "escape_str quotes the value and doubles its quotes");
}

void fuzz_escape_round_trip(FuzzedDataProvider & provider)
{
    const char delimiter = consume_char(provider, delimiters);

    std::string cell = consume_text(provider, plain_chars, 1, 1);
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 15); i > 0; --i)
    {
        const char c = consume_char(provider, cell_chars);
        if (c != '"' || cell.back() != '"')
            cell += c;
    }

    const std::string content = utils::escape_str(cell) + (provider.ConsumeBool() ? "\n" : "");
    const utils::CsvData rows = utils::csv_from_string(content, false, delimiter);
    test::fuzz_check(rows.size() == 1 && rows[0].size() == 1 && rows[0][0] == cell,
                     "an escaped value is parsed back to itself");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_parse_raw(provider);
            break;
        case 1:
            fuzz_parse_rows(provider);
            break;
        case 2:
            fuzz_escape_str(provider);
            break;
        default:
            fuzz_escape_round_trip(provider);
            break;
    }
    return 0;
}
