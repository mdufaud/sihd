#include <sihd/util/Str.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/time.hpp>

#include <sstream>
#include <cstdarg>
#include <mutex>
#include <climits> // LONG_MIN LONG_MAX ULONG_MAX...

#include <cxxabi.h> // demangle
#include <string.h>
#include <errno.h>
#include <math.h> // HUGE_VAL

#ifndef SIHD_UTIL_STR_BUFFER
# define SIHD_UTIL_STR_BUFFER 4096
#endif

namespace sihd::util
{

size_t Str::hexdump_cols = 8;

// format
std::mutex Str::g_buffer_mutex;
const size_t Str::g_buffer_size = SIHD_UTIL_STR_BUFFER;
char Str::g_buffer[SIHD_UTIL_STR_BUFFER];

// escapes sequences - must match
const char Str::g_escapes_open[] = "\"'[({<";
const char Str::g_escapes_close[] = "\"'])}>";
char Str::g_escape_char = '\\';

void    Str::append_sep(std::string & str, std::string_view append, std::string_view sep)
{
    if (str.empty())
        str += append;
    else
    {
        str += sep;
        str += append;
    }
}

char    *Str::csub(std::string_view str, size_t from_idx, ssize_t size)
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

std::string     Str::join(const std::vector<std::string> & join_lst, std::string_view join_with)
{
    std::stringstream ss;
    bool first = true;
    for (const auto & str: join_lst)
    {
        if (!first)
            ss << join_with;
        else
            first = false;
        ss << str;
    }
    return ss.str();
}

std::string     Str::demangle(std::string_view name)
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

std::string     Str::format(std::string_view format, ...)
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

std::string     Str::trim(std::string_view s)
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

std::string &   Str::to_upper(std::string & s)
{
    size_t i = 0;
    while (s[i])
    {
        s[i] = ::toupper(s[i]);
        ++i;
    }
    return s;
}

std::string &   Str::to_lower(std::string & s)
{
    size_t i = 0;
    while (s[i])
    {
        s[i] = ::tolower(s[i]);
        ++i;
    }
    return s;
}

std::string     Str::replace(std::string_view s, std::string_view from, std::string_view to)
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

char    Str::num_to_char(size_t num)
{
    if (num >= 10)
        return 'a' + (num - 10);
    else
        return '0' + num;
}

std::string     Str::to_hex(uint64_t n)
{
    return Str::num_str(n, 16);
}

std::string     Str::to_dec(uint64_t n)
{
    return Str::num_str(n, 10);
}

std::string     Str::to_oct(uint64_t n)
{
    return Str::num_str(n, 8);
}

std::string     Str::num_str(int64_t num, uint16_t base)
{
    if (num == 0)
        return "0";
    std::string ret;
    size_t i = Num::size(num, base);
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

std::string     Str::addr_str(void *addr, size_t padding)
{
    size_t numsize = Num::size((size_t)addr, 16);
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

std::string     Str::hexdump(const void *mem, size_t size, char delim)
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

std::string     Str::hexdump_fmt(const void *mem, size_t size)
{
    size_t i = 0;
    size_t cols = Str::hexdump_cols;
    size_t suppl = size % cols == 0 ? 0 : (cols - (size % cols));
    std::string ret;

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

bool    Str::starts_with(std::string_view s, std::string_view start)
{
    return strncmp(s.data(), start.data(), start.length()) == 0;
}

bool    Str::ends_with(std::string_view s, std::string_view end)
{
    ssize_t ending = s.length() - end.length();
    if (ending < 0)
        return false;
    return strncmp(s.data() + ending, end.data(), end.length()) == 0;
}

bool    Str::is_digit(int c, uint16_t base)
{
    if (base <= 10)
        return base != 0 && c >= '0' && c <= '0' + (base - 1);
    base = base - 10;
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'a' + (base - 1))
        || (c >= 'A' && c <= 'A' + (base - 1));
}

bool    Str::is_number(std::string_view s, uint16_t base)
{
    size_t i = 0;
    const char *data = s.data();
    size_t len = s.length();
    while (i < len)
    {
        if (isspace(data[i]) == 0
            && data[i] != '-'
            && data[i] != '+'
            && is_digit(data[i], base) == false)
            return false;
        ++i;
    }
    return true;
}

std::map<std::string, std::string>  Str::parse_configuration(std::string_view conf)
{
    std::map<std::string, std::string> ret;
    Splitter splitter(";");
    auto split_pairs = splitter.split(conf);
    for (const auto & pair: split_pairs)
    {
        size_t idx = pair.find_first_of('=');
        if (idx != std::string::npos)
        {
            std::string key = pair.substr(0, idx);
            std::string value = pair.substr(idx + 1, pair.size());
            ret[key] = value;
        }
    }
    return ret;
}

bool    Str::to_long(std::string_view str, long *ret, uint16_t base)
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

bool   Str::to_ulong(std::string_view str, unsigned long *ret, uint16_t base)
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

bool    Str::to_llong(std::string_view str, long long *ret, uint16_t base)
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

bool   Str::to_ullong(std::string_view str, unsigned long long *ret, uint16_t base)
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

bool    Str::to_double(std::string_view str, double *ret)
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
bool Str::convert_from_string<bool>(std::string_view str, bool & value, [[maybe_unused]] uint16_t base)
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
        Str::to_lower(lower);
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
bool Str::convert_from_string<char>(std::string_view str, char & value, [[maybe_unused]] uint16_t base)
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
bool Str::convert_from_string<int8_t>(std::string_view str, int8_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int16_t>(std::string_view str, int16_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int32_t>(std::string_view str, int32_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int64_t>(std::string_view str, int64_t & value, uint16_t base)
{
    long long longval;
    bool ret = Str::to_llong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint8_t>(std::string_view str, uint8_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint16_t>(std::string_view str, uint16_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint32_t>(std::string_view str, uint32_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint64_t>(std::string_view str, uint64_t & value, uint16_t base)
{
    unsigned long long longval;
    bool ret = Str::to_ullong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<float>(std::string_view str, float & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    bool ret = Str::to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

template <>
bool Str::convert_from_string<double>(std::string_view str, double & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    bool ret = Str::to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

bool    Str::is_escape_sequence_open(int c, const char *authorized_open_escape_sequences)
{
    const char *search_in = authorized_open_escape_sequences == nullptr ? g_escapes_open : authorized_open_escape_sequences;
    return c > 0 && strchr(search_in, c) != nullptr;
}

bool    Str::is_escape_sequence_close(int c, const char *authorized_close_escape_sequences)
{
    const char *search_in = authorized_close_escape_sequences == nullptr ? g_escapes_close : authorized_close_escape_sequences;
    return c > 0 && strchr(search_in, c) != nullptr;
}

int     Str::closing_escape_of(int c)
{
    const char *open_esc = strchr(g_escapes_open, c);
    if (open_esc != nullptr)
    {
        size_t idx = open_esc - g_escapes_open;
        return g_escapes_close[idx];
    }
    return -1;
}

bool    Str::is_escaped_char(const char *str, int index)
{
    if (index > 0 && str[index - 1] == Str::g_escape_char)
    {
        // if escaped - the escape should not be escaped itself
        if ((index - 2) < 0 || str[index - 2] != Str::g_escape_char)
            return true;
    }
    return false;
}

int     Str::closing_escape_index(const char *str, int index, const char *authorized_open_escape_sequences)
{
    int open_esc = str[index];
    if (authorized_open_escape_sequences != nullptr
        && strchr(authorized_open_escape_sequences, open_esc) == nullptr)
        return -1;
    int close_esc = Str::closing_escape_of(open_esc);
    if (close_esc < 0)
        return -1;
    if (Str::is_escaped_char(str, index))
        return -1;
    int i = index + 1;
    while (str[i])
    {
        if (str[i] == close_esc && Str::is_escaped_char(str, i) == false)
            return i + 1;
        ++i;
    }
    return -2;
}

std::string  Str::remove_escape_char(std::string_view str)
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

std::string  Str::remove_escape_sequences(std::string_view str, const char *authorized_open_escape_sequences)
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
        if (!in_seq
            && Str::is_escape_sequence_open(cstr[i], authorized_open_escape_sequences)
            && Str::is_escaped_char(cstr, i) == false)
        {
            current_seq = Str::closing_escape_of(cstr[i]);
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
        if (!in_seq
            && Str::is_escape_sequence_open(cstr[i], authorized_open_escape_sequences)
            && Str::is_escaped_char(cstr, i) == false)
        {
            current_seq = Str::closing_escape_of(cstr[i]);
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

std::string Str::gmtime_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return Str::_time_to_string(timestamp, total_parenthesis, nano_resolution, false);
}

std::string Str::localtime_str(Timestamp timestamp, bool total_parenthesis, bool nano_resolution)
{
    return Str::_time_to_string(timestamp, total_parenthesis, nano_resolution, true);
}

std::string Str::_time_to_string(time_t nano, bool total_parenthesis, bool nano_resolution, bool localtime)
{
    std::stringstream ss;
    struct tm *tm = time::to_tm(std::abs(nano), localtime);
    if (tm)
    {
        bool next_step;
        ss << (nano > 0 ? "+" : "-");
        if ((next_step = tm->tm_year > 70))
            ss << tm->tm_year - 70 << "y:";
        if ((next_step = next_step || tm->tm_mon > 0))
            ss << tm->tm_mon << "m:";
        if ((next_step = next_step || (tm->tm_mday - 1) > 0))
            ss << tm->tm_mday - 1 << "d::";
        if ((next_step = next_step || tm->tm_hour > 0))
            ss << tm->tm_hour << "h:";
        if ((next_step = next_step || tm->tm_min > 0))
            ss << tm->tm_min << "m:";
        if ((next_step = next_step || tm->tm_sec > 0))
            ss << tm->tm_sec << "s:";
        time_t ms = time::to_milli(nano) % (int)1E3;
        if ((next_step = next_step || ms > 0))
            ss << ms << "ms:";
        time_t us = time::to_micro(nano) % (int)1E3;
        ss << us << "us";
        if (nano_resolution)
        {
            time_t ns = std::abs(nano) % (int)1E3;
            ss << ":" << ns << "ns";
        }
        if (total_parenthesis)
            ss << " (" << nano << ")";
    }
    return ss.str();
}

}