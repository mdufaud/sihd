#include <sihd/util/Str.hpp>

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

size_t  Str::hexdump_cols = 8;

std::mutex      Str::buffer_mutex;
char            Str::buffer[SIHD_UTIL_STR_BUFFER];
const size_t    Str::buffer_size = SIHD_UTIL_STR_BUFFER;

static int   _split_size(const char *s, const char *delimiter, size_t len)
{
    int count = 0;

    int i = 0;
    while (s[i])
    {
        while (s[i] && strncmp(s + i, delimiter, len) == 0)
            i = i + len;
        if (s[i] && s[i] != delimiter[0])
            ++count;
        while (s[i] && strncmp(s + i, delimiter, len) != 0)
            ++i;
    }
    return count;
}

static std::string  _split_get_token(const char *s, int *idx, const char *delimiter, size_t len)
{
    int x = *idx;
    while (s[x] && strncmp(s + x, delimiter, len) == 0)
        x = x + len;
    int y = x;
    while (s[y] && strncmp(s + y, delimiter, len) != 0)
        ++y;
    *idx = y;
    return std::string(s + x, y - x);
}

std::vector<std::string>    Str::split(const std::string & s, const std::string & delimiter)
{
    return Str::split(s, delimiter.c_str());
}

std::vector<std::string>    Str::split(const std::string & str, const char *delimiter)
{
    if (delimiter == nullptr || delimiter[0] == 0)
        return {str};
    size_t dlen = strlen(delimiter);
    const char *s = str.c_str();
    int tokens = _split_size(s, delimiter, dlen);
    std::vector<std::string> ret(tokens);

    int i = 0;
    int j = 0;
    while (tokens-- > 0)
        ret[i++] = _split_get_token(s, &j, delimiter, dlen);
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
        std::lock_guard<std::mutex> l(buffer_mutex);
        size_t ret = vsnprintf(buffer, buffer_size, format, args);
        ret = ret > buffer_size ? buffer_size : ret;
        str.assign(buffer, ret);
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

std::string     Str::hexdump(void *mem, size_t size, char delim)
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

std::string     Str::hexdump_fmt(void *mem, size_t size)
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
    auto split_pairs = Str::split(conf, ";");
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

}