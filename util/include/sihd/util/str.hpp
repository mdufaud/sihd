#ifndef __SIHD_UTIL_STR_HPP__
#define __SIHD_UTIL_STR_HPP__

#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <variant>
#include <vector>

#include <sihd/util/IArray.hpp>
#include <sihd/util/IArrayView.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util::str
{

#if defined(__SIHD_WINDOWS__)
std::string to_str(std::wstring_view view);
std::wstring to_wstr(std::string_view view);
#endif

size_t table_len(char **table);
size_t table_len(const char **table);
std::span<const char *> table_span(char **table);
std::span<const char *> table_span(const char **table);

bool regex_match(std::string_view str, const std::string & pattern);
std::vector<std::string> regex_search(std::string_view str, const std::string & pattern);
std::string regex_replace(const std::string & str, const std::string & pattern, const std::string & replace);
std::vector<std::string> regex_filter(std::span<const std::string> input, const std::string & pattern);
std::vector<std::string> regex_filter(std::span<std::string_view> input, const std::string & pattern);
std::vector<std::string> regex_filter(std::span<const char *> input, const std::string & pattern);

void append_sep(std::string & str, std::string_view append, std::string_view sep = ",");

std::string timeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
std::string localtimeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
// fmt strftime -> "%Y-%m-%d %H:%M:%S"
std::string format_time(Timestamp t, std::string_view format);
std::string format_localtime(Timestamp t, std::string_view format);

std::string word_wrap(std::string_view s, size_t max_width, bool append_hyphen = true);
std::string wrap(std::string_view s, size_t max_width, std::string_view end_with = "...");

// si -> 1K = 1000B
// iec -> 1K = 1024B
std::string bytes_str(int64_t bytes, bool iec = true);

std::string generate_random(size_t size);

bool print(std::string_view str);
bool println(std::string_view str);

// clone str from_idx to size (or all the remaining str)
char *csub(std::string_view str, size_t from_idx, ssize_t size = 0);

std::string demangle(std::string_view name);
template <typename T>
std::string demangle_type_name(const T & type)
{
    return str::demangle(typeid(type).name());
}

std::string format(std::string_view format, ...);

bool is_all_spaces(std::string_view s);
bool is_digit(int c, uint16_t base = 10);
bool is_number(std::string_view s, uint16_t base = 10);
bool starts_with(std::string_view s, std::string_view start, std::string_view suffix = "");
bool ends_with(std::string_view s, std::string_view end, std::string_view prefix = "");

std::vector<std::string> split(std::string_view str);
std::vector<std::string> split(std::string_view str, char delimiter);
std::vector<std::string> split(std::string_view str, std::string_view delimiter);
std::pair<std::string_view, std::string_view> split_pair_view(std::string_view str, std::string_view delimiter);
std::pair<std::string, std::string> split_pair(std::string_view str, std::string_view delimiter);

std::string join(std::initializer_list<std::string_view> list, std::string_view join_str = "\n");
std::string join(std::span<std::string_view> list, std::string_view join_str = "\n");
std::string join(std::span<const std::string> list, std::string_view join_str = "\n");
std::string join(std::span<const char *> list, std::string_view join_str = "\n");

std::string_view ltrim(std::string_view s);
std::string_view rtrim(std::string_view s);
std::string_view trim(std::string_view s);

std::string & to_upper(std::string & s);
std::string & to_lower(std::string & s);
std::string replace(std::string_view s, std::string_view from, std::string_view to);
bool iequals(std::string_view s1, std::string_view s2);

std::string to_hex(uint64_t n);
std::string to_dec(uint64_t n);
std::string to_oct(uint64_t n);
std::string addr_str(const void *addr, size_t padding = 0);
std::string num_str(uint64_t num, uint16_t base);
// 0 -> 9 -> a -> z
char num_to_char(size_t num);

std::string hexdump(const IArray & arr, char delim);
std::string hexdump(const IArrayView & arr, char delim);
std::string hexdump(const void *mem, size_t size, char delim);

// formatted hexdump
std::vector<std::string> hexdump_fmt(const IArray & arr, size_t cols = 8);
std::vector<std::string> hexdump_fmt(const IArrayView & arr, size_t cols = 8);
std::vector<std::string> hexdump_fmt(const void *mem, size_t size, size_t cols = 8);

struct SearchResult
{
        size_t distance;
        std::string word;
};

std::vector<SearchResult> search(std::span<std::string_view> list, const std::string & selection);
std::vector<SearchResult> search(std::span<const std::string> list, const std::string & selection);
std::vector<SearchResult> search(std::span<const char *> list, const std::string & selection);

std::vector<std::string>
    to_columns(std::span<std::string_view> words, size_t max_col_size, std::string_view join_with = " ");
std::vector<std::string>
    to_columns(std::span<const std::string> words, size_t max_col_size, std::string_view join_with = " ");
std::vector<std::string>
    to_columns(std::span<const char *> words, size_t max_col_size, std::string_view join_with = " ");

bool to_long(std::string_view str, long *ret, uint16_t base = 0);
bool to_ulong(std::string_view str, unsigned long *ret, uint16_t base = 0);
bool to_llong(std::string_view str, long long *ret, uint16_t base = 0);
bool to_ullong(std::string_view str, unsigned long long *ret, uint16_t base = 0);
bool to_double(std::string_view str, double *ret);

constexpr const char *encloses_start()
{
    return "\"'[({<";
}

constexpr const char *encloses_stop()
{
    return "\"'])}>";
}

constexpr int escape_char()
{
    return '\\';
}

// simple wrapped find
bool is_char_enclose_start(int c, const char *authorized_start_enclose = encloses_start());
// simple wrapped find
bool is_char_enclose_stop(int c, const char *authorized_stop_enclose = encloses_stop());

/**
 * @brief check if a char is escaped by calculating an impair number of escape '\' before the char
 *  must give the beginning pointer of string and the index of the actual char
 *
 *  example:
 * is_escaped_char("\[hello", 1) == true
 * is_escaped_char("\\[hello", 1) == true
 * is_escaped_char("\\[hello", 2) == false
 */
bool is_escaped_char(const char *str, int index, int escape = escape_char());

// stopping_enclose_of('[') -> ']'
int stopping_enclose_of(int starting_enclose);

// returns the index of the next char after the enclose
// stopping_enclose_index("[hello] world", 0, "[")
//   -> 7  ---------------------^
// stopping_enclose_index("hello", 0, "[")
//   -> -1 (s[0] is not a closing escape)
// stopping_enclose_index("[hello", 0, "[")
//   -> -2 (s[0] has no end to an enclose)
int stopping_enclose_index(std::string_view view,
                           int index,
                           const char *authorized_start_enclose = encloses_start(),
                           int escape = escape_char());

/**
 * find_char_not_escaped("hello ?", '?') -> 6
 * find_char_not_escaped("hello \\?", '?') -> -1
 * find_char_not_escaped("hello \\\\?", '?') -> 8
 */
int find_char_not_escaped(std::string_view view, int char_to_find, int escape = escape_char());

int find_str_not_enclosed(std::string_view origin,
                          std::string_view to_find,
                          const char *authorized_start_enclose = encloses_start(),
                          int escape = escape_char());
// remove_escape_char("hello \\? \\\\?") -> "hello ? \\?"
std::string remove_escape_char(std::string_view str, int escape = escape_char());
// remove_enclosing("hello 'world' \\'!\\'") -> "hello world '!'"
std::string remove_enclosing(std::string_view str,
                             const char *authorized_start_enclose = encloses_start(),
                             int escape = escape_char());

template <typename T>
bool convert_from_string(std::string_view str, T & value, uint16_t base = 0);

} // namespace sihd::util::str

namespace sihd::util
{

template <>
bool str::convert_from_string<bool>(std::string_view str, bool & value, uint16_t base);

template <>
bool str::convert_from_string<char>(std::string_view str, char & value, uint16_t base);

template <>
bool str::convert_from_string<int8_t>(std::string_view str, int8_t & value, uint16_t base);

template <>
bool str::convert_from_string<uint8_t>(std::string_view str, uint8_t & value, uint16_t base);

template <>
bool str::convert_from_string<int16_t>(std::string_view str, int16_t & value, uint16_t base);

template <>
bool str::convert_from_string<uint16_t>(std::string_view str, uint16_t & value, uint16_t base);

template <>
bool str::convert_from_string<int32_t>(std::string_view str, int32_t & value, uint16_t base);

template <>
bool str::convert_from_string<uint32_t>(std::string_view str, uint32_t & value, uint16_t base);

template <>
bool str::convert_from_string<int64_t>(std::string_view str, int64_t & value, uint16_t base);

template <>
bool str::convert_from_string<uint64_t>(std::string_view str, uint64_t & value, uint16_t base);

template <>
bool str::convert_from_string<float>(std::string_view str, float & value, uint16_t base);

template <>
bool str::convert_from_string<double>(std::string_view str, double & value, uint16_t base);

} // namespace sihd::util

#endif
