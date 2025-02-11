#ifndef __SIHD_CSV_UTILS_HPP__
#define __SIHD_CSV_UTILS_HPP__

#include <optional>
#include <tuple>

#include <fmt/ranges.h>

#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::csv::utils
{

SIHD_LOGGER;

using CsvLine = std::vector<std::string>;
using CsvData = std::vector<CsvLine>;

// encloses with quotes and escape quotes in string with another quote
// escape_str("hello \"world\"") -> "\"hello \"\"world\"\"\"
std::string escape_str(std::string_view view);

// get rows/columns from a CSV string
CsvData csv_from_string(std::string_view content, bool remove_header, int delimiter = ',', char comment_char = '#');

// get rows/columns from CSV file
std::optional<CsvData>
    read_csv(std::string_view path, bool remove_header, int delimiter = ',', char comment_char = '#');

// check if tuple size and header size is the same
template <typename... Args>
bool same_number_of_columns(const std::vector<std::string> & columns_tags, const std::vector<std::tuple<Args...>> &)
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

    if (!columns_tags.empty())
        line = fmt::format("{}\n", fmt::join(columns_tags, (const char *)&delimiter));
    for (const auto & row : rows)
    {
        line += fmt::format("{}\n", fmt::join(row, (const char *)&delimiter));
    }
    return line;
}

// write a csv obviously
template <typename... Args>
bool write_csv(std::string_view path,
               const std::vector<std::string> & columns_tags,
               const std::vector<std::tuple<Args...>> & rows,
               int delimiter = ',')
{
    CsvWriter writer(path);

    if (!writer.is_open())
        return false;

    bool success = true;
    std::string line = fmt::format("{}", fmt::join(columns_tags, (const char *)&delimiter));
    ssize_t wrote = writer.write_row(line);

    // add newline to it
    if (wrote != (ssize_t)line.size() + 1)
    {
        SIHD_LOG_ERROR("Failed write on CSV '{}' on header '{}'", path, line);
        success = false;
    }

    for (const auto & row : rows)
    {
        line = fmt::format("{}", fmt::join(row, (const char *)&delimiter));
        wrote = writer.write_row(line);

        // add newline to it
        if (wrote != (ssize_t)line.size() + 1)
        {
            SIHD_LOG_ERROR("Failed write on CSV '{}' on line '{}'", path, line);
            success = false;
            break;
        }
    }

    return success;
}

} // namespace sihd::csv::utils

#endif
