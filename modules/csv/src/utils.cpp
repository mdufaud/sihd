#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/util/str.hpp>

namespace sihd::csv::utils
{

std::string escape_str(std::string_view view)
{
    return fmt::format("\"{}\"", sihd::util::str::replace(view, "\"", "\"\""));
}

CsvData csv_from_string(std::string_view content, bool remove_header, int delimiter, char comment_char)
{
    sihd::util::Splitter rows_splitter;
    rows_splitter.set_delimiter("\n");
    rows_splitter.set_escape_char('\"');
    rows_splitter.set_open_escape_sequences("\"");

    std::vector<CsvLine> ret;
    std::vector<std::string_view> lines = rows_splitter.split_view(content);

    sihd::util::Splitter cols_splitter;
    cols_splitter.set_delimiter_char(delimiter);
    cols_splitter.set_escape_char('\"');
    cols_splitter.set_open_escape_sequences("\"");
    cols_splitter.set_empty_delimitations(true);

    size_t number_of_comments_and_empty_lines = (int)remove_header;
    for (std::string_view & line : lines)
    {
        if (line.empty() || line[0] == comment_char)
            ++number_of_comments_and_empty_lines;
    }
    ret.reserve(lines.size() - number_of_comments_and_empty_lines);

    bool first = true;
    for (std::string_view & line : lines)
    {
        if (first && remove_header)
        {
            first = false;
            continue;
        }

        if (line.empty() || line[0] == comment_char)
            continue;

        std::vector<std::string> columns = cols_splitter.split(line);
        for (std::string & column : columns)
        {
            column = sihd::util::str::remove_escape_char(column, '"');
        }
        ret.emplace_back(std::move(columns));
    }

    return ret;
}

std::expected<CsvData, sihd::util::Error>
    read_csv(std::string_view path, bool remove_header, int delimiter, char comment_char)
{
    CsvReader reader;

    auto res = reader.set_delimiter(delimiter);
    SIHD_UNEXPECTED_RETURN(res);
    auto res_2 = reader.set_commentary(comment_char);
    SIHD_UNEXPECTED_RETURN(res_2);
    auto res_3 = reader.open(path);
    SIHD_UNEXPECTED_RETURN(res_3);

    bool first = true;
    CsvData ret;

    while (true)
    {
        auto line = reader.read_next();
        SIHD_UNEXPECTED_RETURN(line);
        if (*line == false)
            break;
        if (remove_header && first)
        {
            first = false;
            continue;
        }
        ret.emplace_back(reader.columns());
    }

    return ret;
}

} // namespace sihd::csv::utils
