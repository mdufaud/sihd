#include <cxxabi.h> // demangle
#include <errno.h>
#include <math.h> // HUGE_VAL
#include <string.h>

#include <climits> // LONG_MIN LONG_MAX ULONG_MAX...
#include <cstdarg>
#include <mutex>
#include <sstream>

#include <fmt/format.h>

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
std::mutex buffer_mutex;
const size_t buffer_size = SIHD_UTIL_STR_BUFFER;
char buffer[SIHD_UTIL_STR_BUFFER];

std::string _format_time(Timestamp timestamp, std::string_view format, bool localtime)
{
    struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();
    size_t ret = strftime(buffer, buffer_size, format.data(), &tm);
    return std::string(buffer, ret);
}

std::string _timeoffset_to_string(Timestamp timestamp, bool total_parenthesis, bool nano_resolution, bool localtime)
{
    std::string s;
    struct tm tm = localtime ? timestamp.local_tm() : timestamp.tm();
    bool next_step;
    s += (timestamp > 0 ? "+" : "-");
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
        s += fmt::format(" ({})", timestamp);
    return s;
}

} // namespace

const char *escapes_open()
{
    return "\"'[({<";
}

const char *escapes_close()
{
    return "\"'])}>";
}

char escape_char()
{
    return '\\';
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

std::string join(const std::vector<std::string> & join_lst, std::string_view join_with)
{
    std::string s;
    bool first = true;
    for (const auto & str : join_lst)
    {
        if (!first)
            s += join_with;
        else
            first = false;
        s += str;
    }
    return s;
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
        std::lock_guard<std::mutex> l(buffer_mutex);
        size_t ret = vsnprintf(buffer, buffer_size, format.data(), args);
        ret = ret > buffer_size ? buffer_size : ret;
        str.assign(buffer, ret);
    }
    va_end(args);
    return str;
}

std::string trim(std::string_view s)
{
    size_t len = s.size();
    size_t i = 0;
    while (i < len && std::isspace(s[i]))
        ++i;
    size_t j = len;
    while (j > 0 && std::isspace(s[--j]))
        ;
    return std::string(s.substr(i, j - i + 1));
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

std::string addr_str(void *addr, size_t padding)
{
    size_t numsize = num::size((size_t)addr, 16);
    ssize_t i = 0;
    ssize_t total_zero = padding - numsize;
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

std::string hexdump_fmt(const void *mem, size_t size, size_t cols)
{
    std::string ret;
    size_t suppl = size % cols == 0 ? 0 : (cols - (size % cols));

    size_t i = 0;
    while (i < size + suppl)
    {
        if ((i % cols) == 0)
            ret += addr_str((void *)i) + ":\t";
        if (i < size)
        {
            uint16_t hex = 0xFF & ((char *)mem)[i];
            if (hex < 16)
                ret += "0";
            ret += num_str(hex, 16) + " ";
        }
        else
            ret += "   ";
        if ((i % cols) == cols - 1)
        {
            ret += "  ";
            size_t begin = i - (cols - 1);
            while (begin <= i)
            {
                if (begin >= size)
                    ret += " ";
                else if (isprint(((char *)mem)[begin]))
                    ret += ((char *)mem)[begin];
                else
                    ret += ".";
                ++begin;
            }
            ret += "\n";
        }
        ++i;
    }
    return ret;
}

std::string hexdump_fmt(const IArray & arr, size_t cols)
{
    return hexdump_fmt(arr.buf(), arr.byte_size(), cols);
}
std::string hexdump_fmt(const IArrayView & arr, size_t cols)
{
    return hexdump_fmt(arr.buf(), arr.byte_size(), cols);
}

bool starts_with(std::string_view s, std::string_view start)
{
    return strncmp(s.data(), start.data(), start.length()) == 0;
}

bool ends_with(std::string_view s, std::string_view end)
{
    ssize_t ending = s.length() - end.length();
    if (ending < 0)
        return false;
    return strncmp(s.data() + ending, end.data(), end.length()) == 0;
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
    size_t len = s.length();
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
    bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int16_t>(std::string_view str, int16_t & value, uint16_t base)
{
    long longval;
    bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int32_t>(std::string_view str, int32_t & value, uint16_t base)
{
    long longval;
    bool ret = to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<int64_t>(std::string_view str, int64_t & value, uint16_t base)
{
    long long longval;
    bool ret = to_llong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint8_t>(std::string_view str, uint8_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint16_t>(std::string_view str, uint16_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint32_t>(std::string_view str, uint32_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<uint64_t>(std::string_view str, uint64_t & value, uint16_t base)
{
    unsigned long long longval;
    bool ret = to_ullong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool convert_from_string<float>(std::string_view str, float & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    bool ret = to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

template <>
bool convert_from_string<double>(std::string_view str, double & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    bool ret = to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

bool is_escape_sequence_open(int c, const char *authorized_open_escape_sequences)
{
    const char *search_in
        = authorized_open_escape_sequences == nullptr ? escapes_open() : authorized_open_escape_sequences;
    return c > 0 && strchr(search_in, c) != nullptr;
}

bool is_escape_sequence_close(int c, const char *authorized_close_escape_sequences)
{
    const char *search_in
        = authorized_close_escape_sequences == nullptr ? escapes_close() : authorized_close_escape_sequences;
    return c > 0 && strchr(search_in, c) != nullptr;
}

int closing_escape_of(int c)
{
    const char *open_esc = strchr(escapes_open(), c);
    if (open_esc != nullptr)
    {
        size_t idx = open_esc - escapes_open();
        return escapes_close()[idx];
    }
    return -1;
}

bool is_escaped_char(const char *str, int index)
{
    if (index > 0 && str[index - 1] == escape_char())
    {
        // if escaped - the escape should not be escaped itself
        if ((index - 2) < 0 || str[index - 2] != escape_char())
            return true;
    }
    return false;
}

int closing_escape_index(const char *str, int index, const char *authorized_open_escape_sequences)
{
    int open_esc = str[index];
    if (authorized_open_escape_sequences != nullptr && strchr(authorized_open_escape_sequences, open_esc) == nullptr)
        return -1;
    int close_esc = closing_escape_of(open_esc);
    if (close_esc < 0)
        return -1;
    if (is_escaped_char(str, index))
        return -1;
    int i = index + 1;
    while (str[i])
    {
        if (str[i] == close_esc && is_escaped_char(str, i) == false)
            return i + 1;
        ++i;
    }
    return -2;
}

std::string remove_escape_char(std::string_view str)
{
    std::string ret;
    const char *cstr = str.data();
    size_t len = str.size();
    size_t count_escapes = 0;
    size_t i = 0;
    size_t j = 0;

    while (i < len)
    {
        if (cstr[i] == '\\')
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
        if (cstr[i] == '\\')
            ++i;
        if (i < len)
        {
            ret[j] = cstr[i];
            ++i;
            ++j;
        }
    }
    return ret;
}

std::string remove_escape_sequences(std::string_view str, const char *authorized_open_escape_sequences)
{
    const char *cstr = str.data();
    std::string ret;
    size_t len = str.size();
    size_t count_sequences = 0;
    size_t j = 0;
    size_t i = 0;
    int current_seq;
    bool in_seq = false;

    while (i < len)
    {
        if (!in_seq && is_escape_sequence_open(cstr[i], authorized_open_escape_sequences)
            && is_escaped_char(cstr, i) == false)
        {
            current_seq = closing_escape_of(cstr[i]);
            ++count_sequences;
            in_seq = true;
        }
        else if (in_seq && cstr[i] == current_seq)
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
        if (!in_seq && is_escape_sequence_open(cstr[i], authorized_open_escape_sequences)
            && is_escaped_char(cstr, i) == false)
        {
            current_seq = closing_escape_of(cstr[i]);
            ++i;
            in_seq = true;
        }
        else if (in_seq && cstr[i] == current_seq)
        {
            ++i;
            in_seq = false;
        }
        else
        {
            ret[j] = cstr[i];
            ++j;
            ++i;
        }
    }
    return ret;
}

std::string timeoffset_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return _timeoffset_to_string(timestamp, total_parenthesis, nano_resolution, false);
}

std::string localtimeoffset_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return _timeoffset_to_string(timestamp, total_parenthesis, nano_resolution, true);
}

std::string format_time(Timestamp t, std::string_view format)
{
    return _format_time(t, format, false);
}

std::string format_localtime(Timestamp t, std::string_view format)
{
    return _format_time(t, format, true);
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

std::string word_wrap(std::string_view s, size_t width, bool append_hyphen)
{
    if (width == 0)
        return "";
    if (width == 1)
        append_hyphen = false;

    std::string ret;
    size_t i;
    size_t curr_width;
    size_t word_begin;
    size_t word_end;
    bool in_word;

    ret.reserve(s.size() + (s.size() / width));
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

            if (word_size > width)
            {
                // word too big
                if (curr_width > 0)
                    ret.append("\n");
                size_t j = 0;
                size_t add_size;
                while (j < word_size)
                {
                    // -1 for '-' cut
                    if (j + width >= word_size)
                        add_size = word_size - j;
                    else
                        add_size = append_hyphen ? width - 1 : width;
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
            else if ((curr_width + word_size > width) || (curr_width > 0 && curr_width + word_size + 1 > width))
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

} // namespace sihd::util::str
