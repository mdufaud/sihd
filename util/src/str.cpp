#include <cxxabi.h> // demangle
#include <errno.h>
#include <string.h>

#include <algorithm>
#include <climits> // LONG_MIN LONG_MAX ULONG_MAX...
#include <cmath>   // HUGE_VAL
#include <cstdarg>
#include <mutex>
#include <random>
#include <sstream>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/util/IArray.hpp>
#include <sihd/util/IArrayView.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#ifndef SIHD_UTIL_STR_BUFFER
# define SIHD_UTIL_STR_BUFFER 4096
#endif

namespace sihd::util::str
{

namespace
{

// format
std::mutex g_buffer_mutex;
const size_t g_buffer_size = SIHD_UTIL_STR_BUFFER;
char g_buffer[SIHD_UTIL_STR_BUFFER];

std::string format_time(Timestamp timestamp, std::string_view format, bool localtime)
{
    const struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();

    std::lock_guard<std::mutex> l(g_buffer_mutex);
    const size_t ret = strftime(g_buffer, g_buffer_size, format.data(), &tm);
    return std::string(g_buffer, ret);
}

std::string timeoffset_to_string(Timestamp timestamp, bool total_parenthesis, bool nano_resolution, bool localtime)
{
    const struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();
    std::string s;
    bool next_step;

    s += (timestamp >= 0 ? "+" : "-");
    if ((next_step = tm.tm_year > 70))
        s += fmt::format("{}y:", tm.tm_year - 70);
    if ((next_step = next_step || tm.tm_mon > 0))
        s += fmt::format("{}m:", tm.tm_mon);
    if ((next_step = next_step || (tm.tm_mday - 1) > 0))
        s += fmt::format("{}d ", tm.tm_mday - 1);
    if ((next_step = next_step || tm.tm_hour > 0))
        s += fmt::format("{}h:", tm.tm_hour);
    if ((next_step = next_step || tm.tm_min > 0))
        s += fmt::format("{}m:", tm.tm_min);
    if ((next_step = next_step || tm.tm_sec > 0))
        s += fmt::format("{}s:", tm.tm_sec);
    time_t ms = time::to_milli(timestamp) % (int)1E3;
    if ((next_step = next_step || ms > 0))
        s += fmt::format("{}ms:", ms);
    time_t us = time::to_micro(timestamp) % (int)1E3;
    s += fmt::format("{}us", us);
    if (nano_resolution)
    {
        time_t ns = std::abs(timestamp) % (int)1E3;
        s += fmt::format(":{}ns", ns);
    }
    if (total_parenthesis)
        s += fmt::format(" ({})", timestamp.nanoseconds());
    return s;
}

} // namespace

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

char *csub(std::string_view str, size_t from_idx, ssize_t size)
{
    if (size < 0)
        return nullptr;
    char *ret = new char[size + 1];
    size_t idx_src = from_idx;
    size_t idx_dst = 0;

    while (idx_dst < (size_t)size && str[idx_src])
    {
        ret[idx_dst] = str[idx_src];
        ++idx_dst;
        ++idx_src;
    }
    ret[idx_dst] = 0;
    return ret;
}

std::string demangle(std::string_view name)
{
    int status = -1;
    char *ptr = abi::__cxa_demangle(name.data(), NULL, NULL, &status);

    if (status == 0)
    {
        std::string ret = ptr;
        free(ptr);
        return ret;
    }
    return name.data();
}

std::string format(std::string_view format, ...)
{
    std::string str;
    va_list args;

    va_start(args, format);
    {
        std::lock_guard<std::mutex> l(g_buffer_mutex);
        size_t ret = vsnprintf(g_buffer, g_buffer_size, format.data(), args);
        ret = ret > g_buffer_size ? g_buffer_size : ret;
        str.assign(g_buffer, ret);
    }
    va_end(args);
    return str;
}

bool is_all_spaces(std::string_view s)
{
    return std::all_of(s.begin(), s.end(), isspace);
}

std::string_view rtrim(std::string_view s)
{
    size_t j = s.size();

    while (j > 0 && std::isspace(s[--j]))
        ;
    return s.substr(0, j + 1);
}

std::string_view ltrim(std::string_view s)
{
    const size_t len = s.size();
    size_t i = 0;

    while (i < len && std::isspace(s[i]))
        ++i;
    return s.substr(i);
}

std::string_view trim(std::string_view s)
{
    return ltrim(rtrim(s));
}

std::string & to_upper(std::string & s)
{
    size_t i = 0;

    while (s[i])
    {
        s[i] = ::toupper(s[i]);
        ++i;
    }
    return s;
}

std::string & to_lower(std::string & s)
{
    size_t i = 0;

    while (s[i])
    {
        s[i] = ::tolower(s[i]);
        ++i;
    }
    return s;
}

std::string replace(std::string_view s, std::string_view from, std::string_view to)
{
    std::string ret;
    size_t i = s.find(from);
    size_t last = 0;

    while (i != std::string::npos)
    {
        ret += s.substr(last, i - last);
        ret += to;
        i += from.size();
        last = i;
        i = s.find(from, i + 1);
    }
    ret += s.substr(last);
    return ret;
}

bool iequals(std::string_view s1, std::string_view s2)
{
    if (s1.size() != s2.size())
        return false;
    return strncasecmp(s1.data(), s2.data(), s1.size());
}

char num_to_char(size_t num)
{
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
    if (num == 0)
        return "0";
    size_t i = num::size(num, base);
    std::string ret;

    ret.resize(i);
    while (num != 0)
    {
        i--;
        if (num < base)
            ret[i] = num_to_char(num);
        else
            ret[i] = num_to_char(num % base);
        num = num / base;
    }
    return ret;
}

std::string addr_str(const void *addr, size_t padding)
{
    const size_t numsize = num::size((size_t)addr, 16);
    const ssize_t total_zero = padding - numsize;
    ssize_t i = 0;
    std::string ret;

    ret.reserve(2 + numsize + total_zero);
    ret = "0x";
    while (i < total_zero)
    {
        ret += "0";
        ++i;
    }
    ret += num_str((size_t)addr, 16);
    return ret;
}

std::string hexdump(const void *mem, size_t size, char delim)
{
    std::string ret;
    size_t i = 0;

    while (i < size)
    {
        uint16_t hex = 0xFF & ((char *)mem)[i];
        if (delim != '\0' && ret.empty() == false)
            ret += delim;
        if (hex < 16)
            ret += "0";
        ret += num_str(hex, 16);
        ++i;
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
    // add padding if number of columns does not match the size
    const size_t suppl = size % cols == 0 ? 0 : (cols - (size % cols));
    // compute the number of spaces needed
    const size_t max_addr_size = num::size(size, 16) + 1;
    std::vector<std::string> ret;
    std::string line;

    size_t i = 0;
    while (i < size + suppl)
    {
        if ((i % cols) == 0)
            line += fmt::format("{0}:{1:>{2}}", addr_str((void *)i), "", max_addr_size - num::size(i, 16));
        if (i < size)
        {
            uint16_t hex = 0xFF & ((char *)mem)[i];
            if (hex < 16)
                line += "0";
            line += num_str(hex, 16) + " ";
        }
        else
            line += "   ";
        if ((i % cols) == cols - 1)
        {
            line += "  ";
            size_t begin = i - (cols - 1);
            while (begin <= i)
            {
                if (begin >= size)
                    line += " ";
                else if (isprint(((char *)mem)[begin]))
                    line += ((char *)mem)[begin];
                else
                    line += ".";
                ++begin;
            }
            ret.emplace_back(std::move(line));
            line.clear();
            // line += "\n";
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

std::string join(const std::vector<std::string> & list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

std::string join(const std::vector<std::string_view> & list, std::string_view join_str)
{
    return fmt::format("{}", fmt::join(list, join_str));
}

bool starts_with(std::string_view s, std::string_view start, std::string_view suffix)
{
    if (start.length() > (s.length() + suffix.length()))
        return false;
    const bool success = strncmp(s.data(), start.data(), start.length()) == 0;
    if (suffix.empty())
        return success;
    return success && strncmp(s.data() + start.length(), suffix.data(), suffix.length()) == 0;
}

bool ends_with(std::string_view s, std::string_view end, std::string_view prefix)
{
    const ssize_t ending = s.length() - (end.length() + prefix.length());
    if (ending < 0)
        return false;
    const bool success = strncmp(s.data() + ending + prefix.length(), end.data(), end.length()) == 0;
    if (prefix.empty())
        return success;
    return success && strncmp(s.data() + ending, prefix.data(), prefix.length()) == 0;
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
        if (isspace(data[i]) == 0 && data[i] != '-' && data[i] != '+' && is_digit(data[i], base) == false)
            return false;
        ++i;
    }
    return true;
}

bool to_long(std::string_view str, long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtol(str.data(), &endptr, base);
    if (str.data() == endptr)
        return false;
    if (*ret == 0L && errno == EINVAL)
        return false;
    if ((*ret == LONG_MIN || *ret == LONG_MAX) && errno == ERANGE)
        return false;
    return true;
}

bool to_ulong(std::string_view str, unsigned long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtoul(str.data(), &endptr, base);
    if (str.data() == endptr)
        return false;
    if (*ret == 0UL && errno == EINVAL)
        return false;
    if (*ret == ULONG_MAX && errno == ERANGE)
        return false;
    return true;
}

bool to_llong(std::string_view str, long long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtoll(str.data(), &endptr, base);
    if (str.data() == endptr)
        return false;
    if (*ret == 0L && errno == EINVAL)
        return false;
    if ((*ret == LLONG_MIN || *ret == LLONG_MAX) && errno == ERANGE)
        return false;
    return true;
}

bool to_ullong(std::string_view str, unsigned long long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtoull(str.data(), &endptr, base);
    if (str.data() == endptr)
        return false;
    if (*ret == 0UL && errno == EINVAL)
        return false;
    if (*ret == ULLONG_MAX && errno == ERANGE)
        return false;
    return true;
}

bool to_double(std::string_view str, double *ret)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtod(str.data(), &endptr);
    if (str.data() == endptr)
        return false;
    if (*ret == 0 && errno == EINVAL)
        return false;
    if ((*ret == HUGE_VAL || *ret == -HUGE_VAL) && errno == ERANGE)
        return false;
    return true;
}

template <>
bool convert_from_string<bool>(std::string_view str, bool & value, [[maybe_unused]] uint16_t base)
{
    bool ret = false;
    if (str == "1")
    {
        value = true;
        ret = true;
    }
    else if (str == "0")
    {
        value = false;
        ret = true;
    }
    if (!ret)
    {
        std::string lower(str);
        to_lower(lower);
        if (str == "true")
        {
            value = true;
            ret = true;
        }
        else if (str == "false")
        {
            value = false;
            ret = true;
        }
    }
    return ret;
}

template <>
bool convert_from_string<char>(std::string_view str, char & value, [[maybe_unused]] uint16_t base)
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

template <>
bool convert_from_string<int8_t>(std::string_view str, int8_t & value, uint16_t base)
{
    long longval;
    const bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int16_t>(std::string_view str, int16_t & value, uint16_t base)
{
    long longval;
    const bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int32_t>(std::string_view str, int32_t & value, uint16_t base)
{
    long longval;
    const bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int64_t>(std::string_view str, int64_t & value, uint16_t base)
{
    long long longval;
    const bool ret = to_llong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint8_t>(std::string_view str, uint8_t & value, uint16_t base)
{
    unsigned long longval;
    const bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint16_t>(std::string_view str, uint16_t & value, uint16_t base)
{
    unsigned long longval;
    const bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint32_t>(std::string_view str, uint32_t & value, uint16_t base)
{
    unsigned long longval;
    const bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint64_t>(std::string_view str, uint64_t & value, uint16_t base)
{
    unsigned long long longval;
    const bool ret = to_ullong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<float>(std::string_view str, float & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    const bool ret = to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

template <>
bool convert_from_string<double>(std::string_view str, double & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    const bool ret = to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

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
    const int open_esc = view[index];
    if (view[0] == 0 || (authorized_start_enclose != nullptr && strchr(authorized_start_enclose, open_esc) == nullptr))
        return -1;

    const int stopping_enclose = stopping_enclose_of(open_esc);
    const bool is_an_escape
        = escape == stopping_enclose && ((size_t)index + 1 < view.size() && view[index + 1] == stopping_enclose);

    if (stopping_enclose < 0 || is_an_escape || is_escaped_char(view.data(), index, escape))
        return -1;

    size_t i = index + 1;
    while (i < view.size())
    {
        if (view[i] == stopping_enclose)
        {
            const bool is_an_escape
                = escape == stopping_enclose && (i + 1 < view.size() && view[i + 1] == stopping_enclose);
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

int find_str_not_enclosed(std::string_view origin,
                          std::string_view to_find,
                          const char *authorized_start_enclose,
                          int escape)
{
    size_t i = 0;

    while (i < origin.size())
    {
        int closed_at = stopping_enclose_index(origin.data(), i, authorized_start_enclose, escape);
        if (closed_at > 0)
        {
            // matched closure - skip it
            i = closed_at;
        }
        else if (closed_at == -2)
        {
            // never ends - not possible to find a string not escaped here
            return -1;
        }
        else
        {
            // not a closing escape
            if (origin.compare(i, to_find.size(), to_find) == 0)
                return i;
            ++i;
        }
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
    return format_time(t, format, false);
}

std::string format_localtime(Timestamp t, std::string_view format)
{
    return format_time(t, format, true);
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
        return std::to_string(bytes < 0 ? 0 : bytes) + "B";
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

    ssize_t rest = ((bytes % current_scale) / (float)current_scale) * 10;
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
    size_t i;
    size_t curr_width;
    size_t word_begin;
    size_t word_end;
    bool in_word;

    ret.reserve(s.size() + (s.size() / max_width));
    i = 0;
    curr_width = 0;
    word_begin = 0;
    word_end = 0;
    in_word = false;
    while (i < s.size())
    {
        bool should_add = false;
        if (isspace(s[i]))
        {
            if (in_word)
            {
                // not in a word but was in a word last idx
                word_end = i;
                in_word = false;
                should_add = true;
            }
        }
        else /* isspace() == false */
        {
            if (in_word == false)
            {
                // in a word but was not in a word last idx
                word_begin = i;
                in_word = true;
            }
        }
        const bool next_is_end = i + 1 >= s.size();
        if (should_add || (in_word && next_is_end))
        {
            // not in a word but was in a word last idx
            if (next_is_end)
                word_end = i + 1;
            size_t word_size = word_end - word_begin;

            if (word_size > max_width)
            {
                // word too big
                if (curr_width > 0)
                    ret.append("\n");
                size_t j = 0;
                size_t add_size;
                while (j < word_size)
                {
                    // -1 for '-' cut
                    if (j + max_width >= word_size)
                        add_size = word_size - j;
                    else
                        add_size = append_hyphen ? max_width - 1 : max_width;
                    ret.append(s.data() + word_begin + j, add_size);
                    if (j + add_size < word_size)
                    {
                        if (append_hyphen)
                            ret.append("-\n");
                        else
                            ret.push_back('\n');
                    }
                    j += add_size;
                    curr_width = add_size;
                }
            }
            else if ((curr_width + word_size > max_width) || (curr_width > 0 && curr_width + word_size + 1 > max_width))
            {
                ret.append("\n");
                ret.append(s.data() + word_begin, word_size);
                curr_width = word_size;
            }
            else
            {
                if (curr_width > 0)
                {
                    ret.push_back(' ');
                    ++curr_width;
                }
                ret.append(s.data() + word_begin, word_size);
                curr_width += word_size;
            }
        }
        if (s[i] == '\n' && !next_is_end)
        {
            ret.push_back('\n');
            curr_width = 0;
        }
        ++i;
    }
    return ret;
}

std::string generate_random(size_t size)
{
    constexpr char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n\t ()[]{}'123456789!@#$%^&*_+";

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, sizeof(charset) - 1);

    std::string str;
    str.resize(size);
    for (size_t i = 0; i < size; ++i)
        str[i] = charset[dist(rng) % (sizeof(charset) - 1)];
    str[size] = 0;
    return str;
}

std::string wrap(std::string_view s, size_t max_width, std::string_view end_with)
{
    if (max_width < end_with.size())
        return std::string(end_with.data(), max_width);

    max_width = max_width - end_with.size();

    if (s.size() < max_width)
        return std::string(s.data(), s.size());

    return fmt::format("{}{}", std::string_view(s.data(), max_width - end_with.size()), end_with);
}

std::vector<std::string> split(std::string_view str)
{
    Splitter splitter;
    splitter.set_delimiter_spaces();
    return splitter.split(str);
}

std::vector<std::string>
    to_columns(const std::vector<std::string> & words, size_t max_width, std::string_view join_with)
{
    std::vector<std::string> ret;

    if (words.empty())
        return {};

    const size_t max_possible_lines = words.size();

    std::vector<size_t> column_size;
    column_size.resize(max_possible_lines);

    /**
     * Check every column combinaison to check if we are inside the maximum width
     */
    size_t selected_nb_lines = 0;
    size_t selected_max_line_size = 0;
    for (size_t line_index = 1; line_index <= max_possible_lines; ++line_index)
    {
        const size_t nb_columns = std::ceil(words.size() / (float)line_index);
        const size_t nb_word_per_columns = line_index;

        bool good = true;

        size_t current_line_size = 0;
        size_t column_index = 0;
        while (column_index < nb_columns)
        {
            const size_t begin_word_index = nb_word_per_columns * column_index;

            size_t biggest_column_word_size = 0;
            size_t i = 0;
            while (i < nb_word_per_columns)
            {
                if (begin_word_index + i >= words.size())
                    break;
                biggest_column_word_size = std::max(biggest_column_word_size, words[begin_word_index + i].size());
                ++i;
            }

            current_line_size += biggest_column_word_size;
            // take the joiner into account
            if (column_index > 0)
                current_line_size += join_with.size();

            if (current_line_size > max_width)
            {
                good = false;
                break;
            }

            column_size[column_index] = biggest_column_word_size;

            ++column_index;
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

    const size_t nb_columns = std::ceil(words.size() / (float)selected_nb_lines);

    ret.reserve(selected_nb_lines);

    size_t line_index = 0;
    while (line_index < selected_nb_lines)
    {
        std::string str;
        str.reserve(selected_max_line_size);

        size_t column_index = 0;
        while (column_index < nb_columns)
        {
            const size_t word_index = (selected_nb_lines * column_index) + line_index;

            if (word_index >= words.size())
                break;

            if (column_index > 0)
                str += join_with;

            str += fmt::format("{0:<{1}}", words[word_index], column_size[column_index]);

            ++column_index;
        }

        ret.emplace_back(str);
        ++line_index;
    }

    return ret;
}

} // namespace sihd::util::str
