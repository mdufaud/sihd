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

void    Str::append_sep(std::string & str, const std::string & append, const std::string & sep)
{
    if (str.empty())
        str += append;
    else
        str += sep + append;
}

char    *Str::csub(const char *str, size_t from_idx, ssize_t size)
{
    if (size == 0)
        size = strlen(str) - from_idx;
    if (size < 0)
        return nullptr;
    char *ret = new char[size + 1];
    size_t idx_src = from_idx;
    size_t idx_dst = 0;
    while (str[idx_src])
    {
        ret[idx_dst] = str[idx_src];
        ++idx_dst;
        ++idx_src;
    }
    ret[idx_dst] = 0;
    return ret;
}

std::string     Str::join(const std::vector<std::string> & join_lst, const std::string & join_with)
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

std::string     Str::demangle(const char *name)
{
    int status = -1;
    char *ptr = abi::__cxa_demangle(name, NULL, NULL, &status);
    if (status == 0)
    {
        std::string ret = ptr;
        free(ptr);
        return ret;
    }
    return name;
}

std::string     Str::format(const char *format, ...)
{
    std::string str;
    va_list args;
    va_start(args, format);
    {
        std::lock_guard<std::mutex> l(g_buffer_mutex);
        size_t ret = vsnprintf(g_buffer, g_buffer_size, format, args);
        ret = ret > g_buffer_size ? g_buffer_size : ret;
        str.assign(g_buffer, ret);
    }
    va_end(args);
    return str;
}

std::string     Str::trim(const std::string & s)
{
    size_t i = 0;
    while (std::isspace(s[i]))
        ++i;
    size_t j = s.length();
    while (j > 0 && std::isspace(s[--j]))
        ;
    return s.substr(i, j - i + 1);
}

std::string &   Str::to_upper(std::string & s)
{
    size_t i = 0;
    std::locale loc;
    while (s[i])
    {
        s[i] = std::toupper(s[i], loc);
        ++i;
    }
    return s;
}

std::string &   Str::to_lower(std::string & s)
{
    size_t i = 0;
    std::locale loc;
    while (s[i])
    {
        s[i] = std::tolower(s[i], loc);
        ++i;
    }
    return s;
}

std::string     Str::replace(const std::string & s, const std::string & from, const std::string & to)
{
    std::string ret;
    size_t i = s.find(from);
    size_t last = 0;
    while (i != std::string::npos)
    {
        ret += s.substr(last, i - last) + to;
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

std::string     Str::num_to_string(size_t num, uint16_t base)
{
    size_t i = 0;
    size_t size = Num::get_size(num, base);
    std::string ret;
    while (i < size)
    {
        if (num < base)
            ret = num_to_char(num) + ret;
        else
        {
            ret = num_to_char(num % base) + ret;
            num = num / base;
        }
        ++i;
    }
    return ret;
}

std::string     Str::addr_to_string(void *addr, size_t padding)
{
    size_t numsize = Num::get_size((size_t)addr, 16);
    ssize_t i = 0;
    ssize_t total_zero = padding - numsize;
    std::string ret = "0x";
    while (i < total_zero)
    {
        ret += "0";
        ++i;
    }
    ret += num_to_string((size_t)addr, 16);
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
        ret += num_to_string(hex, 16);
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
            ret += addr_to_string((void *)i) + ":\t";
        if (i < size)
        {
            uint16_t hex = 0xFF & ((char *)mem)[i];
            if (hex < 16)
                ret += "0";
            ret += num_to_string(hex, 16) + " ";
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

bool    Str::starts_with(const std::string & s, const std::string & start)
{
    return strncmp(s.c_str(), start.c_str(), start.length()) == 0;
}

bool    Str::ends_with(const std::string & s, const std::string & end)
{
    ssize_t ending = s.length() - end.length();
    if (ending < 0)
        return false;
    return strncmp(s.c_str() + ending, end.c_str(), end.length()) == 0;
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

bool    Str::is_number(const std::string & s, uint16_t base)
{
    size_t i = 0;
    const char *c_str = s.c_str();
    size_t len = s.length();
    while (i < len)
    {
        if (isspace(c_str[i]) == 0
            && c_str[i] != '-'
            && c_str[i] != '+'
            && is_digit(c_str[i], base) == false)
            return false;
        ++i;
    }
    return true;
}

std::map<std::string, std::string>  Str::parse_configuration(const std::string & conf)
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

bool    Str::to_long(const std::string & str, long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtol(str.c_str(), &endptr, base);
    if (str.c_str() == endptr)
        return false;
    if (*ret == 0L && errno == EINVAL)
        return false;
    if ((*ret == LONG_MIN || *ret == LONG_MAX) && errno == ERANGE)
        return false;
    return true;
}

bool   Str::to_ulong(const std::string & str, unsigned long *ret, uint16_t base)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtoul(str.c_str(), &endptr, base);
    if (str.c_str() == endptr)
        return false;
    if (*ret == 0UL && errno == EINVAL)
        return false;
    if (*ret == ULONG_MAX && errno == ERANGE)
        return false;
    return true;
}

bool    Str::to_double(const std::string & str, double *ret)
{
    errno = 0;
    char *endptr = NULL;
    *ret = strtod(str.c_str(), &endptr);
    if (str.c_str() == endptr)
        return false;
    if (*ret == 0 && errno == EINVAL)
        return false;
    if ((*ret == HUGE_VAL || *ret == -HUGE_VAL) && errno == ERANGE)
        return false;
    return true;
}

template <>
bool Str::convert_from_string<bool>(const std::string & str, bool & value, [[maybe_unused]] uint16_t base)
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
        std::string lower = str;
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
bool Str::convert_from_string<char>(const std::string & str, char & value, [[maybe_unused]] uint16_t base)
{
    if (str.size() == 1)
        value = str[0];
    return str.size() == 1;
}

template <>
bool Str::convert_from_string<int8_t>(const std::string & str, int8_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int16_t>(const std::string & str, int16_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int32_t>(const std::string & str, int32_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<int64_t>(const std::string & str, int64_t & value, uint16_t base)
{
    long longval;
    bool ret = Str::to_long(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint8_t>(const std::string & str, uint8_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint16_t>(const std::string & str, uint16_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint32_t>(const std::string & str, uint32_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<uint64_t>(const std::string & str, uint64_t & value, uint16_t base)
{
    unsigned long longval;
    bool ret = Str::to_ulong(str, &longval, base);
    if (ret)
        value = longval;
    return ret;
}

template <>
bool Str::convert_from_string<float>(const std::string & str, float & value, [[maybe_unused]] uint16_t base)
{
    double doubleval;
    bool ret = Str::to_double(str, &doubleval);
    if (ret)
        value = doubleval;
    return ret;
}

template <>
bool Str::convert_from_string<double>(const std::string & str, double & value, [[maybe_unused]] uint16_t base)
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

int     Str::get_closing_escape_index(const char *str, int index, const char *authorized_open_escape_sequences)
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

std::string  Str::remove_escape_char(const std::string & str)
{
    const char *cstr = str.c_str();
    std::string ret;
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

std::string  Str::remove_escape_sequences(const std::string & str, const char *authorized_open_escape_sequences)
{
    const char *cstr = str.c_str();
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

std::string Str::gmtime_to_string(time_t nano, bool total_parenthesis, bool nano_resolution)
{
    return Str::_time_to_string(nano, total_parenthesis, nano_resolution, false);
}

std::string Str::localtime_to_string(time_t nano, bool total_parenthesis, bool nano_resolution)
{
    return Str::_time_to_string(nano, total_parenthesis, nano_resolution, true);
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