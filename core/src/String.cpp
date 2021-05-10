#include <sihd/core/String.hpp>
#include <string.h>
#include <cxxabi.h>
#include <sstream>

namespace sihd::core::str
{

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

std::vector<std::string>    split(const std::string & str, const char *delimiter)
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

std::string     join(const std::vector<std::string> & join_lst, const std::string & join_with)
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

std::string     demangle(const char *name)
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

}