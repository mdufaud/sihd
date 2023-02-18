#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <string_view>

# include <sihd/util/Timestamp.hpp>
# include <sihd/util/IArray.hpp>
# include <sihd/util/IArrayView.hpp>

namespace sihd::util::str
{

const char *escapes_open();
const char *escapes_close();
char escape_char();

void append_sep(std::string & str, std::string_view append, std::string_view sep = ",");

std::string timeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
std::string localtimeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
// fmt strftime -> "%Y-%m-%d %H:%M:%S"
std::string format_time(Timestamp t, std::string_view format);
std::string format_localtime(Timestamp t, std::string_view format);

std::string word_wrap(std::string_view s, size_t width, bool append_hyphen = true);

// si -> 1K = 1000B
// iec -> 1K = 1024B
std::string bytes_str(ssize_t bytes, bool iec = true);

bool is_escape_sequence_open(int c, const char *authorized_open_escape_sequences = nullptr);
bool is_escape_sequence_close(int c, const char *authorized_close_escape_sequences = nullptr);
/**
 * @brief check if a char is escaped by calculating an impair number of escape '\' before the char
 *  must give the beginning pointer of string and the index of the actual char
 *
 *  example:
 * str = "\[hello";
 * is_escaped_char(str, 1) == true
 * str = "\\[hello";
 * is_escaped_char(str, 1) == true
 * is_escaped_char(str, 2) == false
 */
bool is_escaped_char(const char *str, int index);
// '[' -> ']'
int closing_escape_of(int sequence);
// [hello] -> 7
int closing_escape_index(const char *s, int index, const char *authorized_open_escape_sequences = nullptr);

std::string remove_escape_char(std::string_view str);
std::string remove_escape_sequences(std::string_view str, const char *authorized_open_escape_sequences = nullptr);

// clone str from_idx to size (or all the remaining str)
char *csub(std::string_view str, size_t from_idx, ssize_t size = 0);

std::string demangle(std::string_view name);
std::string format(std::string_view format, ...);

std::string trim(std::string_view s);
std::string & to_upper(std::string & s);
std::string & to_lower(std::string & s);
std::string replace(std::string_view s, std::string_view from, std::string_view to);
bool iequals(std::string_view s1, std::string_view s2);

std::string to_hex(uint64_t n);
std::string to_dec(uint64_t n);
std::string to_oct(uint64_t n);
std::string addr_str(void *addr, size_t padding = 0);
std::string num_str(uint64_t num, uint16_t base);
char num_to_char(size_t num);

std::string hexdump(const IArray & arr, char delim);
std::string hexdump(const IArrayView & arr, char delim);
std::string hexdump(const void *mem, size_t size, char delim);

// formatted hexdump

std::string hexdump_fmt(const IArray & arr, size_t cols = 8);
std::string hexdump_fmt(const IArrayView & arr, size_t cols = 8);
std::string hexdump_fmt(const void *mem, size_t size, size_t cols = 8);

bool is_digit(int c, uint16_t base = 10);
bool is_number(std::string_view s, uint16_t base = 10);
bool starts_with(std::string_view s, std::string_view start);
bool ends_with(std::string_view s, std::string_view end);

bool to_long(std::string_view str, long *ret, uint16_t base = 0);
bool to_ulong(std::string_view str, unsigned long *ret, uint16_t base = 0);
bool to_llong(std::string_view str, long long *ret, uint16_t base = 0);
bool to_ullong(std::string_view str, unsigned long long *ret, uint16_t base = 0);
bool to_double(std::string_view str, double *ret);

template <typename T>
bool convert_from_string(std::string_view str, T & value, uint16_t base = 0);

}

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

}

#endif
