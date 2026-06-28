#ifndef __SIHD_CSV_UTILS_HPP__
#define __SIHD_CSV_UTILS_HPP__

#include <optional>
#include <stdexcept>
#include <tuple>

#include <fmt/ranges.h>

#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::csv::utils
{

using CsvLine = std::vector<std::string>;
using CsvData = std::vector<CsvLine>;

// encloses with quotes and escape quotes in string with another quote
// escape_str("hello \"world\"") -> "\"hello \"\"world\"\"\"
std::string escape_str(std::string_view view);

// get rows/columns from a CSV string
CsvData csv_from_string(std::string_view content,
                        bool remove_header,
                        int delimiter = ',',
                        char comment_char = '#');

// get rows/columns from CSV file
std::optional<CsvData>
    read_csv(std::string_view path, bool remove_header, int delimiter = ',', char comment_char = '#');

// check if tuple size and header size is the same
template <typename... Args>
bool same_number_of_columns(const std::vector<std::string> & columns_tags,
                            const std::vector<std::tuple<Args...>> &)
{
    return columns_tags.size() == sizeof...(Args);
}

// get a string from rows/columns
template <typename... Args>
std::string csv_to_str(const std::vector<std::string> & columns_tags,
                       const std::vector<std::tuple<Args...>> & rows,
                       int delimiter = ',')
{
    std::string line;
    const std::string delim(1, static_cast<char>(delimiter));

    if (!columns_tags.empty())
        line = fmt::format("{}\n", fmt::join(columns_tags, delim));
    for (const auto & row : rows)
    {
        line += fmt::format("{}\n", fmt::join(row, delim));
    }
    return line;
}

template <typename... Args>
void write_csv(std::string_view path,
               const std::vector<std::string> & columns_tags,
               const std::vector<std::tuple<Args...>> & rows,
               int delimiter = ',')
{
    CsvWriter writer(path);

    if (!writer.is_open())
        throw std::runtime_error(fmt::format("Failed to open CSV '{}' for writing", path));

    const std::string delim(1, static_cast<char>(delimiter));
    std::string line = fmt::format("{}", fmt::join(columns_tags, delim));
    ssize_t wrote = writer.write_row(line);

    if (wrote != (ssize_t)line.size() + 1)
        throw std::runtime_error(fmt::format("Failed write on CSV '{}' on header '{}'", path, line));

    for (const auto & row : rows)
    {
        line = fmt::format("{}", fmt::join(row, delim));
        wrote = writer.write_row(line);

        if (wrote != (ssize_t)line.size() + 1)
            throw std::runtime_error(fmt::format("Failed write on CSV '{}' on line '{}'", path, line));
    }
}

} // namespace sihd::csv::utils

#endif
