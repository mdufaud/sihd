#include <cxxabi.h> // demangle
#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <climits> // LONG_MIN LONG_MAX ULONG_MAX...
#include <cmath>   // HUGE_VAL
#include <cstdarg>
#include <iomanip> // std::put_time
#include <iterator>
#include <locale>
#include <random>
#include <regex>
#include <sstream>

#include <fmt/ranges.h>

#include <sihd/util/IArray.hpp>
#include <sihd/util/IArrayView.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#ifndef SIHD_UTIL_STR_BUFFER
# define SIHD_UTIL_STR_BUFFER 1024
#endif

#if defined(__SIHD_WINDOWS__)
# include <stringapiset.h>
#endif

namespace sihd::util::str
{

namespace
{

size_t levenshtein_distance(std::string_view source,
                            std::string_view target,
                            size_t insert_cost,
                            size_t delete_cost,
                            size_t replace_cost,
                            std::vector<size_t> & lev_dist)
{
    // https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance
    if (source.size() > target.size())
    {
        return levenshtein_distance(target, source, delete_cost, insert_cost, replace_cost, lev_dist);
    }

    const size_t min_size = source.size();
    const size_t max_size = target.size();
    lev_dist.resize(min_size + 1);

    lev_dist[0] = 0;
    for (size_t i = 1; i <= min_size; ++i)
    {
        lev_dist[i] = lev_dist[i - 1] + delete_cost;
    }

    for (size_t j = 1; j <= max_size; ++j)
    {
        size_t previous_diagonal = lev_dist[0], previous_diagonal_save;
        lev_dist[0] += insert_cost;

        const char tc = target[j - 1];
        for (size_t i = 1; i <= min_size; ++i)
        {
            previous_diagonal_save = lev_dist[i];
            const char sc = source[i - 1];
            if (sc == tc)
            {
                lev_dist[i] = previous_diagonal;
            }
            else
            {
                lev_dist[i] = std::min(std::min(lev_dist[i - 1] + delete_cost, lev_dist[i] + insert_cost),
                                       previous_diagonal + replace_cost);
            }
            previous_diagonal = previous_diagonal_save;
        }
    }

    return lev_dist[min_size];
}

std::string format_time(Timestamp timestamp,
                        std::string_view format,
                        bool localtime,
                        [[maybe_unused]] const std::locale *loc = nullptr)
{
#if !defined(__SIHD_EMSCRIPTEN__)
    if (loc != nullptr)
    {
        std::ostringstream oss;
        oss.imbue(*loc);
        const struct tm tm_val = localtime ? timestamp.local_tm() : timestamp.tm();
        oss << std::put_time(&tm_val, format.data());
        return oss.str();
    }
    else
#endif
    {
        constexpr size_t buffer_size = SIHD_UTIL_STR_BUFFER;
        thread_local char buffer[buffer_size];

        const struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();
        const size_t ret = strftime(buffer, buffer_size, format.data(), &tm);
        return std::string(buffer, ret);
    }
}

std::string timeoffset_to_string(Timestamp timestamp, bool total_parenthesis, bool nano_resolution, bool localtime)
{
    const struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();
    std::string s;
    s.reserve(64);
    bool next_step;

    s += (timestamp >= 0 ? "+" : "-");
    if ((next_step = tm.tm_year > 70))
        fmt::format_to(std::back_inserter(s), "{}y:", tm.tm_year - 70);
    if ((next_step = next_step || tm.tm_mon > 0))
        fmt::format_to(std::back_inserter(s), "{}m:", tm.tm_mon);
    if ((next_step = next_step || (tm.tm_mday - 1) > 0))
        fmt::format_to(std::back_inserter(s), "{}d ", tm.tm_mday - 1);
    if ((next_step = next_step || tm.tm_hour > 0))
        fmt::format_to(std::back_inserter(s), "{}h:", tm.tm_hour);
    if ((next_step = next_step || tm.tm_min > 0))
        fmt::format_to(std::back_inserter(s), "{}m:", tm.tm_min);
    if ((next_step = next_step || tm.tm_sec > 0))
        fmt::format_to(std::back_inserter(s), "{}s:", tm.tm_sec);
    time::UnixTime ms = time::to_milli(timestamp) % 1000;
    if ((next_step = next_step || ms > 0))
        fmt::format_to(std::back_inserter(s), "{}ms:", ms);
    time::UnixTime us = time::to_micro(timestamp) % 1000;
    fmt::format_to(std::back_inserter(s), "{}us", us);
    if (nano_resolution)
    {
        time::UnixTime ns = std::abs(timestamp) % 1000;
        fmt::format_to(std::back_inserter(s), ":{}ns", ns);
    }
    if (total_parenthesis)
        fmt::format_to(std::back_inserter(s), " ({})", timestamp.nanoseconds());
    return s;
}

template <typename T>
std::vector<SearchResult> search_impl(std::span<T> list, const std::string & selection)
{
    std::vector<SearchResult> ret;
    ret.reserve(list.size());

    constexpr size_t insert_cost = 3;
    constexpr size_t delete_cost = 4;
    constexpr size_t replace_cost = 2;

    // strings are lowered once and buffers reused across the list
    std::string selection_lower = selection;
    str::to_lower(selection_lower);
    std::string word_lower;
    std::vector<size_t> lev_dist;

    for (const auto & entry : list)
    {
        word_lower = std::string_view(entry);
        str::to_lower(word_lower);
        ret.emplace_back(SearchResult {
            .distance = levenshtein_distance(word_lower,
                                             selection_lower,
                                             insert_cost,
                                             delete_cost,
                                             replace_cost,
                                             lev_dist),
            .word = std::string(std::string_view(entry)),
        });
    }
    std::stable_sort(ret.begin(), ret.end(), [](const auto & a, const auto & b) {
        if (a.distance != b.distance)
            return a.distance < b.distance;
        return a.word < b.word;
    });
    return ret;
}

template <typename T>
std::vector<std::string> to_columns_impl(std::span<T> words, size_t max_width, std::string_view join_with)
{
    std::vector<std::string> ret;

    if (words.empty())
        return {};

    const size_t max_possible_lines = words.size();

    std::vector<size_t> column_size;
    column_size.resize(max_possible_lines);

    size_t selected_nb_lines = 0;
    size_t selected_max_line_size = 0;
    for (size_t line_index = 1; line_index <= max_possible_lines; ++line_index)
    {
        const size_t nb_columns = (words.size() + line_index - 1) / line_index;
        const size_t nb_word_per_columns = line_index;

        bool good = true;

        size_t current_line_size = 0;
        for (size_t column_index = 0; column_index < nb_columns; ++column_index)
        {
            const size_t begin_word_index = nb_word_per_columns * column_index;

            size_t biggest_column_word_size = 0;
            for (size_t i = 0; i < nb_word_per_columns; ++i)
            {
                if (begin_word_index + i >= words.size())
                    break;
                biggest_column_word_size = std::max(biggest_column_word_size,
                                                    std::string_view(words[begin_word_index + i]).size());
            }

            current_line_size += biggest_column_word_size;
            if (column_index > 0)
                current_line_size += join_with.size();

            if (current_line_size > max_width)
            {
                good = false;
                break;
            }

            column_size[column_index] = biggest_column_word_size;
        }

        if (good)
        {
            selected_max_line_size = current_line_size;
            selected_nb_lines = line_index;
            break;
        }
    }

    if (selected_nb_lines == 0)
        return {};

    const size_t nb_columns = (words.size() + selected_nb_lines - 1) / selected_nb_lines;

    ret.reserve(selected_nb_lines);

    for (size_t line_index = 0; line_index < selected_nb_lines; ++line_index)
    {
        std::string str;
        str.reserve(selected_max_line_size);

        for (size_t column_index = 0; column_index < nb_columns; ++column_index)
        {
            const size_t word_index = (selected_nb_lines * column_index) + line_index;

            if (word_index >= words.size())
                break;

            if (column_index > 0)
                str += join_with;

            const std::string_view word(words[word_index]);
            str += word;
            str.append(column_size[column_index] - word.size(), ' ');
        }

        ret.emplace_back(str);
    }

    return ret;
}

template <typename T>
std::vector<std::string> regex_filter_impl(std::span<T> input, const std::string & pattern)
{
    std::regex regex_pattern(pattern);
    std::vector<std::string> output;
    for (const auto & entry : input)
    {
        const std::string_view view(entry);
        if (std::regex_search(view.begin(), view.end(), regex_pattern))
        {
            output.emplace_back(view);
        }
    }
    return output;
}

char glob_lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

// POSIX bracket classes: [:alpha:], [:digit:]...
bool glob_posix_class_match(std::string_view name, char c)
{
    static constexpr std::pair<std::string_view, int (*)(int)> classes[] = {
        {"alnum", std::isalnum},
        {"alpha", std::isalpha},
        {"blank", std::isblank},
        {"cntrl", std::iscntrl},
        {"digit", std::isdigit},
        {"graph", std::isgraph},
        {"lower", std::islower},
        {"print", std::isprint},
        {"punct", std::ispunct},
        {"space", std::isspace},
        {"upper", std::isupper},
        {"xdigit", std::isxdigit},
    };
    for (const auto & [class_name, is_type] : classes)
    {
        if (class_name == name)
            return is_type(static_cast<unsigned char>(c)) != 0;
    }
    return false;
}

// parses the character class starting at pattern[i] ('[') and tells if c is a member;
// on match i is left past the closing ']' or right after '[' when the class is unterminated
bool glob_match_class(std::string_view pattern, size_t & i, char c)
{
    const size_t start = i;
    size_t j = start + 1;
    bool negate = false;
    if (j < pattern.size() && pattern[j] == '!')
    {
        negate = true;
        ++j;
    }
    // fnmatch: ']' first in class is a literal member
    const size_t body = j;
    bool member = false;
    while (j < pattern.size())
    {
        if (pattern[j] == ']' && j > body)
        {
            if (member != negate)
            {
                i = j + 1;
                return true;
            }
            return false;
        }
        if (pattern[j] == '[' && j + 1 < pattern.size() && pattern[j + 1] == ':')
        {
            const size_t stop = pattern.find(":]", j + 2);
            if (stop != std::string_view::npos)
            {
                if (glob_posix_class_match(pattern.substr(j + 2, stop - j - 2), c))
                    member = true;
                j = stop + 2;
                continue;
            }
        }
        char first = pattern[j];
        if (first == '\\' && j + 1 < pattern.size())
            first = pattern[++j];
        char last = first;
        size_t element_end = j;
        // fnmatch: '-' is literal at class edges
        if (j + 2 < pattern.size() && pattern[j + 1] == '-' && pattern[j + 2] != ']')
        {
            last = pattern[j + 2];
            element_end = j + 2;
            if (last == '\\' && j + 3 < pattern.size())
            {
                last = pattern[j + 3];
                element_end = j + 3;
            }
        }
        if (c >= first && c <= last)
            member = true;
        j = element_end + 1;
    }
    // fnmatch: unterminated '[' is a literal
    if (c != '[')
        return false;
    i = start + 1;
    return true;
}

// advances p past the matched pattern element: '?', class, escaped char or literal
bool glob_match_element(std::string_view pattern, size_t & p, char c)
{
    char literal = pattern[p];
    bool escaped = false;
    size_t next = p + 1;
    if (literal == '\\')
    {
        // fnmatch: trailing escape is a literal backslash
        if (next == pattern.size())
        {
            if (c != '\\')
                return false;
            p = next;
            return true;
        }
        literal = pattern[next++];
        escaped = true;
    }
    if (!escaped)
    {
        if (literal == '?')
        {
            p = next;
            return true;
        }
        if (literal == '[')
            return glob_match_class(pattern, p, c);
    }
    if (literal != c)
        return false;
    p = next;
    return true;
}

// pattern must already be lowered when matching case-insensitively
bool glob_match_impl(std::string_view str, std::string_view pattern, bool ignore_case)
{
    size_t si = 0;
    size_t pi = 0;
    size_t star_pi = std::string_view::npos;
    size_t star_si = 0;
    while (si < str.size())
    {
        const char c = ignore_case ? glob_lower(str[si]) : str[si];
        if (pi < pattern.size() && pattern[pi] != '*' && glob_match_element(pattern, pi, c))
        {
            ++si;
            continue;
        }
        if (pi < pattern.size() && pattern[pi] == '*')
        {
            star_pi = pi++;
            star_si = si;
            continue;
        }
        if (star_pi == std::string_view::npos)
            return false;
        pi = star_pi + 1;
        si = ++star_si;
    }
    while (pi < pattern.size() && pattern[pi] == '*')
        ++pi;
    return pi == pattern.size();
}

std::string_view glob_lower_pattern(std::string_view pattern, std::string & lower_pattern)
{
    lower_pattern = pattern;
    to_lower(lower_pattern);
    return lower_pattern;
}

template <typename T>
std::vector<std::string> glob_filter_impl(std::span<T> input, std::string_view pattern, bool ignore_case)
{
    // the pattern is lowered once for the whole list
    std::string lower_pattern;
    if (ignore_case)
        pattern = glob_lower_pattern(pattern, lower_pattern);

    std::vector<std::string> output;
    for (const auto & entry : input)
    {
        if (glob_match_impl(entry, pattern, ignore_case))
            output.emplace_back(entry);
    }
    return output;
}

// strips a base prefix (0x, 0b, 0o) when it matches the requested base and returns the base to use
// base == 0 auto-detects from the prefix: 0x -> 16, 0b -> 2, 0o -> 8, otherwise 10
uint16_t resolve_base_prefix(std::string_view & str, uint16_t base)
{
    if (str.size() >= 2 && str[0] == '0')
    {
        uint16_t prefix_base = 0;
        switch (str[1])
        {
            case 'x':
            case 'X':
                prefix_base = 16;
                break;
            case 'b':
            case 'B':
                prefix_base = 2;
                break;
            case 'o':
            case 'O':
                prefix_base = 8;
                break;
            default:
                break;
        }
        if (prefix_base != 0 && (base == 0 || base == prefix_base))
        {
            str.remove_prefix(2);
            return prefix_base;
        }
    }
    return base == 0 ? 10 : base;
}

template <typename T, typename... Args>
std::optional<T> from_chars_to(std::string_view str, Args... args)
{
    T value {};
    const char *const first = str.data();
    const char *const last = first + str.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, args...);
    if (ec != std::errc {} || ptr == first)
        return std::nullopt;
    return value;
}

#if !(defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)

std::optional<double> strtod_fallback(std::string_view str)
{
    // string_view is not guaranteed null-terminated, copy before strtod
    const std::string tmp(str);
    errno = 0;
    char *endptr = nullptr;
    const double val = strtod(tmp.c_str(), &endptr);
    if (endptr == tmp.c_str())
        return std::nullopt;
    if (val == 0 && errno == EINVAL)
        return std::nullopt;
    if ((val == HUGE_VAL || val == -HUGE_VAL) && errno == ERANGE)
        return std::nullopt;
    return val;
}

#endif

} // namespace

#if defined(__SIHD_WINDOWS__)
// clang-format off
std::string to_str(std::wstring_view utf16_view)
{
    const int size = WideCharToMultiByte(CP_UTF8, 0, utf16_view.data(), utf16_view.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8_str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16_view.data(), utf16_view.size(), utf8_str.data(), size, nullptr, nullptr);
    return utf8_str;
}

std::wstring to_wstr(std::string_view utf8_view)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8_view.data(), utf8_view.size(), nullptr, 0);
    std::wstring utf16_str(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_view.data(), utf8_view.size(), utf16_str.data(), size);
    return utf16_str;
}

std::u32string to_u32str(std::string_view utf8_view)
{
    const std::wstring utf16_str = to_wstr(utf8_view);
    std::u32string utf32_str;
    utf32_str.reserve(utf16_str.size());
    for (size_t i = 0; i < utf16_str.size(); ++i)
    {
        const char32_t unit = static_cast<char32_t>(static_cast<uint16_t>(utf16_str[i]));
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < utf16_str.size())
        {
            const char32_t low = static_cast<char32_t>(static_cast<uint16_t>(utf16_str[i + 1]));
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                utf32_str.push_back(0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
                ++i;
                continue;
            }
        }
        utf32_str.push_back(unit);
    }
    return utf32_str;
}
// clang-format on
#endif

size_t table_len(const char **table)
{
    if (table == nullptr)
        throw std::invalid_argument("table_len: table cannot be nullptr");
    const char **ptr = table;
    while (*ptr)
        ++ptr;
    return ptr - table;
}

size_t table_len(char **table)
{
    return table_len(const_cast<const char **>(table));
}

std::span<const char *> table_span(char **table)
{
    return std::span<const char *>(const_cast<const char **>(table), str::table_len(table));
}

std::span<const char *> table_span(const char **table)
{
    return std::span<const char *>(table, str::table_len(table));
}

bool regex_match(std::string_view str, const std::string & pattern)
{
    std::regex regex_pattern(pattern);
    return std::regex_match(str.begin(), str.end(), regex_pattern);
}

std::vector<std::string> regex_search(std::string_view str, const std::string & pattern)
{
    std::regex regex_pattern(pattern);
    using sv_iterator = std::string_view::const_iterator;
    std::regex_iterator<sv_iterator> it(str.begin(), str.end(), regex_pattern);
    const decltype(it) end;

    std::vector<std::string> matches;
    while (it != end)
    {
        matches.push_back(it->str());
        ++it;
    }
    return matches;
}

std::string regex_replace(const std::string & str, const std::string & pattern, const std::string & replace)
{
    std::regex regex_pattern(pattern);
    return std::regex_replace(str, regex_pattern, replace);
}

std::vector<std::string> regex_filter(std::span<const std::string> input, const std::string & pattern)
{
    return regex_filter_impl(input, pattern);
}

std::vector<std::string> regex_filter(std::span<std::string_view> input, const std::string & pattern)
{
    return regex_filter_impl(input, pattern);
}

std::vector<std::string> regex_filter(std::span<const char *> input, const std::string & pattern)
{
    return regex_filter_impl(input, pattern);
}

bool glob_match(std::string_view str, std::string_view pattern, bool ignore_case)
{
    std::string lower_pattern;
    if (ignore_case)
        pattern = glob_lower_pattern(pattern, lower_pattern);
    return glob_match_impl(str, pattern, ignore_case);
}

std::vector<std::string> glob_filter(std::span<const std::string> input, std::string_view pattern, bool ignore_case)
{
    return glob_filter_impl(input, pattern, ignore_case);
}

std::vector<std::string> glob_filter(std::span<std::string_view> input, std::string_view pattern, bool ignore_case)
{
    return glob_filter_impl(input, pattern, ignore_case);
}

std::vector<std::string> glob_filter(std::span<const char *> input, std::string_view pattern, bool ignore_case)
{
    return glob_filter_impl(input, pattern, ignore_case);
}

std::vector<std::string> split(std::string_view str)
{
    Splitter splitter;
    splitter.set_delimiter_spaces();
    return splitter.split(str);
}

std::vector<std::string> split(std::string_view str, char delimiter)
{
    Splitter splitter;
    splitter.set_delimiter_char(delimiter);
    return splitter.split(str);
}

std::vector<std::string> split(std::string_view str, std::string_view delimiter)
{
    Splitter splitter;
    splitter.set_delimiter(delimiter);
    return splitter.split(str);
}

std::pair<std::string_view, std::string_view> split_pair_view(std::string_view str, std::string_view delimiter)
{
    std::pair<std::string_view, std::string_view> ret;

    size_t idx = str.find_first_of(delimiter);
    if (idx != std::string_view::npos)
    {
        ret.first = str.substr(0, idx);
        ret.second = str.substr(idx + delimiter.length());
    }
    return ret;
}

std::pair<std::string, std::string> split_pair(std::string_view str, std::string_view delimiter)
{
    std::pair<std::string, std::string> ret;

    auto [key, value] = split_pair_view(str, delimiter);
    if (!key.empty())
    {
        ret.first = key;
        ret.second = value;
    }

    return ret;
}

void append_sep(std::string & str, std::string_view append, std::string_view sep)
{
    if (str.empty())
        str += append;
    else
    {
        str += sep;
        str += append;
    }
}

char *csub(std::string_view str, Slice slice)
{
    auto range = slice.resolve(str.size());
    if (range.empty())
        return nullptr;
    char *ret = new char[range.size() + 1];
    memcpy(ret, str.data() + range.from, range.size());
    ret[range.size()] = 0;
    return ret;
}

std::string demangle(std::string_view name)
{
    int status = -1;
    char *ptr = abi::__cxa_demangle(name.data(), NULL, NULL, &status);

    if (status == 0 && ptr != nullptr)
    {
        std::string ret = ptr;
        free(ptr);
        return ret;
    }
    return std::string(name);
}

std::string format(std::string_view format, ...)
{
    constexpr size_t buffer_size = SIHD_UTIL_STR_BUFFER;
    thread_local char buffer[buffer_size];

    va_list args;
    va_start(args, format);
    const int size = vsnprintf(buffer, buffer_size, format.data(), args);
    va_end(args);

    if (size < 0)
        return "";
    if ((size_t)size < buffer_size)
        return std::string(buffer, size);

    // larger than the buffer: args were consumed, restart and format into the result
    va_start(args, format);
    std::string str(size, 0);
    vsnprintf(str.data(), size + 1, format.data(), args);
    va_end(args);
    return str;
}

bool is_all_spaces(std::string_view s)
{
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
}

std::string_view rtrim(std::string_view s)
{
    size_t j = s.size();

    while (j > 0 && std::isspace(static_cast<unsigned char>(s[--j])))
        ;
    return s.substr(0, j + 1);
}

std::string_view ltrim(std::string_view s)
{
    const size_t len = s.size();
    size_t i = 0;

    while (i < len && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
    return s.substr(i);
}

std::string_view trim(std::string_view s)
{
    return ltrim(rtrim(s));
}

std::string & to_upper(std::string & s)
{
    for (char & c : s)
        c = ::toupper(static_cast<unsigned char>(c));
    return s;
}

std::string & to_lower(std::string & s)
{
    for (char & c : s)
        c = ::tolower(static_cast<unsigned char>(c));
    return s;
}

std::string replace(std::string_view s, std::string_view from, std::string_view to)
{
    if (from.empty())
        return std::string(s);

    std::string ret;
    size_t i = s.find(from);
    size_t last = 0;

    while (i != std::string_view::npos)
    {
        ret += s.substr(last, i - last);
        ret += to;
        i += from.size();
        last = i;
        i = s.find(from, i);
    }
    ret += s.substr(last);
    return ret;
}

bool iequals(std::string_view s1, std::string_view s2)
{
    if (s1.size() != s2.size())
        return false;
    return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
}

char num_to_char(size_t num)
{
    if (num >= 36)
        return 0;
    if (num >= 10)
        return 'a' + (num - 10);
    else
        return '0' + num;
}

std::string to_hex(uint64_t n)
{
    return num_str(n, 16);
}

std::string to_dec(uint64_t n)
{
    return num_str(n, 10);
}

std::string to_oct(uint64_t n)
{
    return num_str(n, 8);
}

std::string num_str(uint64_t num, uint16_t base)
{
    if (base < 2 || base > 36)
        return "";
    // max 64 digits (uint64_t in base 2)
    char buffer[64];
    const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), num, static_cast<int>(base));
    return std::string(buffer, ptr);
}

std::string addr_str(const void *addr, size_t padding)
{
    return fmt::format("0x{:0{}x}", (size_t)addr, padding);
}

std::string hexdump(const void *mem, size_t size, char delim)
{
    std::string ret;
    ret.reserve(size * 3);
    const unsigned char *bytes = (const unsigned char *)mem;

    for (size_t i = 0; i < size; ++i)
    {
        if (delim != '\0' && i > 0)
            ret += delim;
        ret += num_to_char(bytes[i] / 16);
        ret += num_to_char(bytes[i] % 16);
    }
    return ret;
}

std::string hexdump(const IArray & arr, char delim)
{
    return hexdump(arr.buf(), arr.byte_size(), delim);
}
std::string hexdump(const IArrayView & arr, char delim)
{
    return hexdump(arr.buf(), arr.byte_size(), delim);
}

std::vector<std::string> hexdump_fmt(const void *mem, size_t size, size_t cols)
{
    if (cols == 0)
        return {};

    const size_t suppl = size % cols == 0 ? 0 : (cols - (size % cols));
    const size_t max_addr_size = num::size(size, 16) + 1;
    std::vector<std::string> ret;
    std::string line;
    const unsigned char *bytes = (const unsigned char *)mem;

    size_t i = 0;
    while (i < size + suppl)
    {
        if ((i % cols) == 0)
        {
            fmt::format_to(std::back_inserter(line), "0x{:x}:", i);
            line.append(max_addr_size - num::size(i, 16), ' ');
        }
        if (i < size)
        {
            line += num_to_char(bytes[i] / 16);
            line += num_to_char(bytes[i] % 16);
            line += ' ';
        }
        else
        {
            line += "   ";
        }
        if ((i % cols) == cols - 1)
        {
            line += "  ";
            for (size_t begin = i - (cols - 1); begin <= i; ++begin)
            {
                if (begin >= size)
                    line += ' ';
                else if (std::isprint(bytes[begin]))
                    line += (char)bytes[begin];
                else
                    line += '.';
            }
            ret.emplace_back(std::move(line));
            line.clear();
        }
        ++i;
    }
    return ret;
}

std::vector<std::string> hexdump_fmt(const IArray & arr, size_t cols)
{
    return hexdump_fmt(arr.buf(), arr.byte_size(), cols);
}
std::vector<std::string> hexdump_fmt(const IArrayView & arr, size_t cols)
{
    return hexdump_fmt(arr.buf(), arr.byte_size(), cols);
}

bool print(std::string_view str)
{
    try
    {
        fmt::print("{}", str);
    }
    catch (const std::system_error &)
    {
        return false;
    }
    return true;
}

bool println(std::string_view str)
{
    try
    {
        fmt::print("{}\n", str);
    }
    catch (const std::system_error &)
    {
        return false;
    }
    return true;
}

std::string join(std::initializer_list<std::string_view> list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

std::string join(std::span<std::string_view> list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

std::string join(std::span<const std::string> list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

std::string join(std::span<const char *> list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

bool starts_with(std::string_view s, std::string_view start, std::string_view suffix)
{
    if (s.starts_with(start) == false)
        return false;
    return suffix.empty() || s.substr(start.size()).starts_with(suffix);
}

bool ends_with(std::string_view s, std::string_view end, std::string_view prefix)
{
    if (s.ends_with(end) == false)
        return false;
    return prefix.empty() || s.substr(0, s.size() - end.size()).ends_with(prefix);
}

bool is_digit(int c, uint16_t base)
{
    if (base <= 10)
        return base != 0 && c >= '0' && c <= '0' + (base - 1);
    base = base - 10;
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'a' + (base - 1)) || (c >= 'A' && c <= 'A' + (base - 1));
}

bool is_number(std::string_view s, uint16_t base)
{
    size_t i = 0;
    const char *data = s.data();
    const size_t len = s.length();
    while (i < len)
    {
        if (std::isspace(static_cast<unsigned char>(data[i])) == 0 && data[i] != '-' && data[i] != '+'
            && is_digit(data[i], base) == false)
            return false;
        ++i;
    }
    return true;
}

std::optional<long long> to_signed(std::string_view str, uint16_t base)
{
    if (base != 0 && (base < 2 || base > 36))
        return std::nullopt;
    // std::from_chars handles neither a leading '+' nor a base prefix, strtol did
    bool negate = false;
    if (!str.empty() && (str.front() == '-' || str.front() == '+'))
    {
        negate = str.front() == '-';
        str.remove_prefix(1);
    }
    base = resolve_base_prefix(str, base);
    const auto opt = from_chars_to<long long>(str, static_cast<int>(base));
    if (!opt)
        return std::nullopt;
    return negate ? -*opt : *opt;
}

std::optional<unsigned long long> to_unsigned(std::string_view str, uint16_t base)
{
    if (base != 0 && (base < 2 || base > 36))
        return std::nullopt;
    // std::from_chars rejects a sign for unsigned, strtoul wrapped a negative input around
    bool negate = false;
    if (!str.empty() && (str.front() == '-' || str.front() == '+'))
    {
        negate = str.front() == '-';
        str.remove_prefix(1);
    }
    base = resolve_base_prefix(str, base);
    const auto opt = from_chars_to<unsigned long long>(str, static_cast<int>(base));
    if (!opt)
        return std::nullopt;
    return negate ? static_cast<unsigned long long>(0) - *opt : *opt;
}

bool to_bool(std::string_view str, bool & value)
{
    if (str == "1")
    {
        value = true;
        return true;
    }
    if (str == "0")
    {
        value = false;
        return true;
    }
    if (str == "true")
    {
        value = true;
        return true;
    }
    if (str == "false")
    {
        value = false;
        return true;
    }
    return false;
}

bool to_char(std::string_view str, char & value)
{
    char c = 0;
    if (str.size() == 1)
        c = str[0];
    else if (str.size() == 3 && str[0] == '\'' && str[2] == '\'')
        c = str[1];
    else
        return false;
    if (isprint(c))
        value = c;
    return c != 0;
}

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L

std::optional<float> to_float(std::string_view str, std::chars_format fmt)
{
    return from_chars_to<float>(str, fmt);
}

std::optional<double> to_double(std::string_view str, std::chars_format fmt)
{
    return from_chars_to<double>(str, fmt);
}

#else

std::optional<float> to_float(std::string_view str, [[maybe_unused]] std::chars_format fmt)
{
    const auto opt = strtod_fallback(str);
    if (!opt)
        return std::nullopt;
    return static_cast<float>(*opt);
}

std::optional<double> to_double(std::string_view str, [[maybe_unused]] std::chars_format fmt)
{
    return strtod_fallback(str);
}

#endif

bool is_char_enclose_start(int c, const char *authorized_start_enclose)
{
    return c > 0 && strchr(authorized_start_enclose, c) != nullptr;
}

bool is_char_enclose_stop(int c, const char *authorized_stop_enclose)
{
    return c > 0 && strchr(authorized_stop_enclose, c) != nullptr;
}

int stopping_enclose_of(int c)
{
    const char *open_esc = strchr(encloses_start(), c);
    if (open_esc != nullptr)
    {
        size_t idx = open_esc - encloses_start();
        return encloses_stop()[idx];
    }
    return -1;
}

bool is_escaped_char(const char *str, int index, int escape)
{
    const bool is_escaped = index > 0 && str[index - 1] == escape;
    const bool is_escape_escaped = index > 1 && str[index - 2] == escape;
    return is_escaped && !is_escape_escaped;
}

int find_char_not_escaped(std::string_view view, int char_to_find, int escape)
{
    size_t idx = 0;

    while ((idx = view.find(char_to_find, idx)) != std::string_view::npos)
    {
        if (is_escaped_char(view.data(), idx, escape) == false)
            return idx;
        ++idx;
    }
    return -1;
}

int stopping_enclose_index(std::string_view view, int index, const char *authorized_start_enclose, int escape)
{
    if (index >= (int)view.size())
        return -1;

    const int open_esc = view[index];
    if (authorized_start_enclose != nullptr && strchr(authorized_start_enclose, open_esc) == nullptr)
        return -1;

    const int stopping_enclose = stopping_enclose_of(open_esc);
    const bool is_an_escape = escape == stopping_enclose
                              && ((size_t)index + 1 < view.size() && view[index + 1] == stopping_enclose);

    if (stopping_enclose < 0 || is_an_escape || is_escaped_char(view.data(), index, escape))
        return -1;

    size_t i = index + 1;
    while (i < view.size())
    {
        if (view[i] == stopping_enclose)
        {
            const bool is_an_escape = escape == stopping_enclose
                                      && (i + 1 < view.size() && view[i + 1] == stopping_enclose);
            if (!is_an_escape && is_escaped_char(view.data(), i, escape) == false)
                return i + 1;
        }
        ++i;
    }
    return -2;
}

std::string remove_escape_char(std::string_view str, int escape)
{
    const size_t len = str.size();
    size_t count_escapes = 0;
    size_t i = 0;
    size_t j = 0;
    std::string ret;

    while (i < len)
    {
        if (str[i] == escape)
        {
            ++count_escapes;
            ++i;
        }
        ++i;
    }
    ret.resize(len - count_escapes);
    i = 0;
    while (i < len)
    {
        if (str[i] == escape)
            ++i;
        if (i < len)
        {
            ret[j] = str[i];
            ++i;
            ++j;
        }
    }
    return ret;
}

std::string remove_enclosing(std::string_view str, const char *authorized_start_enclose, int escape)
{
    const size_t len = str.size();
    size_t count_sequences = 0;
    size_t j = 0;
    size_t i = 0;
    bool in_seq = false;
    int current_seq;
    std::string ret;

    while (i < len)
    {
        if (!in_seq && is_char_enclose_start(str[i], authorized_start_enclose)
            && is_escaped_char(str.data(), i, escape) == false)
        {
            current_seq = stopping_enclose_of(str[i]);
            ++count_sequences;
            in_seq = true;
        }
        else if (in_seq && str[i] == current_seq)
        {
            ++count_sequences;
            in_seq = false;
        }
        ++i;
    }
    ret.resize(len - count_sequences);
    i = 0;
    in_seq = false;
    while (i < len)
    {
        if (!in_seq && is_char_enclose_start(str[i], authorized_start_enclose)
            && is_escaped_char(str.data(), i, escape) == false)
        {
            current_seq = stopping_enclose_of(str[i]);
            ++i;
            in_seq = true;
        }
        else if (in_seq && str[i] == current_seq)
        {
            ++i;
            in_seq = false;
        }
        else
        {
            ret[j] = str[i];
            ++j;
            ++i;
        }
    }
    return ret;
}

std::string_view unquote(std::string_view str)
{
    str = trim(str);
    if (str.size() >= 2 && (str.front() == '"' || str.front() == '\'') && str.back() == str.front())
        return str.substr(1, str.size() - 2);
    return str;
}

int find_str_not_enclosed(std::string_view origin,
                          std::string_view to_find,
                          const char *authorized_start_enclose,
                          int escape)
{
    size_t i = 0;

    while (i < origin.size())
    {
        int closed_at = stopping_enclose_index(origin, (int)i, authorized_start_enclose, escape);
        if (closed_at > 0)
        {
            // matched closure - skip it
            i = (size_t)closed_at;
            continue;
        }
        if (closed_at == -2)
        {
            // never ends - not possible to find a string not escaped here
            return -1;
        }
        // i is not an enclosure start: search up to the next enclosure start only, matches may straddle it
        const size_t next_start = authorized_start_enclose != nullptr
                                      ? origin.find_first_of(authorized_start_enclose, i + 1)
                                      : std::string_view::npos;
        const size_t segment_size = (next_start != std::string_view::npos ? next_start : origin.size()) - i;
        const size_t look_size = std::min(origin.size() - i, segment_size + to_find.size() - (to_find.empty() ? 0 : 1));
        const size_t hit = origin.substr(i, look_size).find(to_find);
        if (hit != std::string_view::npos && hit < segment_size)
        {
            return (int)(i + hit);
        }
        if (next_start == std::string_view::npos)
        {
            return -1;
        }
        i = next_start;
    }
    return -1;
}

std::string timeoffset_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return timeoffset_to_string(timestamp, total_parenthesis, nano_resolution, false);
}

std::string localtimeoffset_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return timeoffset_to_string(timestamp, total_parenthesis, nano_resolution, true);
}

std::string format_time(Timestamp t, std::string_view format)
{
    return format_time(t, format, false, nullptr);
}

std::string format_localtime(Timestamp t, std::string_view format)
{
    return format_time(t, format, true, nullptr);
}

std::string format_time(Timestamp t, std::string_view format, const std::locale & loc)
{
    return format_time(t, format, false, &loc);
}

std::string format_localtime(Timestamp t, std::string_view format, const std::locale & loc)
{
    return format_time(t, format, true, &loc);
}

std::string bytes_str(int64_t bytes, bool iec)
{
    constexpr int64_t kbyte_si = 1000;
    constexpr int64_t mbyte_si = kbyte_si * 1000;
    constexpr int64_t gbyte_si = mbyte_si * 1000;
    constexpr int64_t tbyte_si = gbyte_si * 1000;
    constexpr int64_t kbyte_iec = 1024;
    constexpr int64_t mbyte_iec = kbyte_iec * 1024;
    constexpr int64_t gbyte_iec = mbyte_iec * 1024;
    constexpr int64_t tbyte_iec = gbyte_iec * 1024;

    if (bytes < 0)
        return "";

    int64_t kbyte = iec ? kbyte_iec : kbyte_si;
    int64_t mbyte = iec ? mbyte_iec : mbyte_si;
    int64_t gbyte = iec ? gbyte_iec : gbyte_si;
    int64_t tbyte = iec ? tbyte_iec : tbyte_si;

    char scale_key;
    int64_t current_scale;

    if (bytes < kbyte)
        return std::to_string(bytes) + "B";
    else if (bytes < mbyte)
    {
        scale_key = 'K';
        current_scale = kbyte;
    }
    else if (bytes < gbyte)
    {
        scale_key = 'M';
        current_scale = mbyte;
    }
    else if (bytes < tbyte)
    {
        scale_key = 'G';
        current_scale = gbyte;
    }
    else
    {
        scale_key = 'T';
        current_scale = tbyte;
    }

    const ssize_t rest = ((bytes % current_scale) * 10) / current_scale;
    if (rest > 0)
        return fmt::format("{}.{}{}", bytes / current_scale, rest, scale_key);
    return fmt::format("{}{}", bytes / current_scale, scale_key);
}

std::string word_wrap(std::string_view s, size_t max_width, bool append_hyphen)
{
    if (max_width == 0)
        return "";
    if (max_width == 1)
        append_hyphen = false;

    std::string ret;
    ret.reserve(s.size() + (s.size() / max_width));

    size_t line_width = 0;
    size_t word_begin = 0;
    bool in_word = false;

    const auto append_word = [&](size_t begin, size_t end) {
        const size_t word_size = end - begin;
        if (word_size > max_width)
        {
            if (line_width > 0)
                ret += '\n';
            size_t offset = 0;
            while (offset < word_size)
            {
                const size_t chunk_size = offset + max_width >= word_size ? word_size - offset
                                                                          : (append_hyphen ? max_width - 1 : max_width);
                ret.append(s.data() + begin + offset, chunk_size);
                if (offset + chunk_size < word_size)
                    ret += append_hyphen ? "-\n" : "\n";
                offset += chunk_size;
                line_width = chunk_size;
            }
        }
        else if (line_width > 0 && line_width + word_size + 1 > max_width)
        {
            ret += '\n';
            ret.append(s.data() + begin, word_size);
            line_width = word_size;
        }
        else
        {
            if (line_width > 0)
            {
                ret += ' ';
                ++line_width;
            }
            ret.append(s.data() + begin, word_size);
            line_width += word_size;
        }
    };

    for (size_t i = 0; i < s.size(); ++i)
    {
        if (std::isspace(static_cast<unsigned char>(s[i])))
        {
            if (in_word)
            {
                in_word = false;
                // a whitespace terminating the string is part of the word
                append_word(word_begin, i + 1 == s.size() ? i + 1 : i);
            }
            if (s[i] == '\n' && i + 1 < s.size())
            {
                ret += '\n';
                line_width = 0;
            }
        }
        else
        {
            if (in_word == false)
            {
                word_begin = i;
                in_word = true;
            }
            if (i + 1 == s.size())
                append_word(word_begin, i + 1);
        }
    }
    return ret;
}

std::string generate_random(size_t size)
{
    constexpr std::string_view
        charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n\t ()[]{}'123456789!@#$%^&*_+";

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);

    std::string str;
    str.reserve(size);
    for (size_t i = 0; i < size; ++i)
        str.push_back(charset[dist(rng)]);
    return str;
}

std::string wrap(std::string_view s, size_t max_width, std::string_view end_with)
{
    if (max_width < end_with.size())
        return std::string(end_with.data(), max_width);

    max_width = max_width - end_with.size();

    if (s.size() < max_width)
        return std::string(s.data(), s.size());

    const size_t content_size = max_width > end_with.size() ? max_width - end_with.size() : 0;
    std::string ret(s.data(), content_size);
    ret += end_with;
    return ret;
}

std::vector<std::string> to_columns(std::span<std::string_view> words, size_t max_width, std::string_view join_with)
{
    return to_columns_impl(words, max_width, join_with);
}

std::vector<std::string> to_columns(std::span<const std::string> words, size_t max_width, std::string_view join_with)
{
    return to_columns_impl(words, max_width, join_with);
}

std::vector<std::string> to_columns(std::span<const char *> words, size_t max_width, std::string_view join_with)
{
    return to_columns_impl(words, max_width, join_with);
}

std::vector<SearchResult> search(std::span<std::string_view> list, const std::string & selection)
{
    return search_impl(list, selection);
}

std::vector<SearchResult> search(std::span<const std::string> list, const std::string & selection)
{
    return search_impl(list, selection);
}

std::vector<SearchResult> search(std::span<const char *> list, const std::string & selection)
{
    return search_impl(list, selection);
}

} // namespace sihd::util::str
