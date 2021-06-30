#include <sihd/util/Str.hpp>
#include <string.h>
#include <cxxabi.h>
#include <sstream>
#include <cstdarg>
#include <mutex>

#define SIHD_UTIL_STR_BUFFER 20480

namespace sihd::util
{

std::mutex      Str::buffer_mutex;
const size_t    Str::buffer_size = SIHD_UTIL_STR_BUFFER;
char            Str::buffer[SIHD_UTIL_STR_BUFFER];

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
    std::string ret;
    va_list args;
    va_start(args, format);
    {
        std::lock_guard<std::mutex> l(buffer_mutex);
        vsnprintf(buffer, buffer_size, format, args);
        ret.assign(buffer, strlen(buffer));
    }
    va_end(args);
    return ret;
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

}