#include <sihd/util/Splitter.hpp>
#include <sihd/util/Logger.hpp>

#include <string.h>

namespace sihd::util
{

SIHD_LOGGER;

Splitter::Splitter(): _empty_delimitations(false), _compare_method(nullptr)
{
}

Splitter::Splitter(int delimiter, std::string_view authorized_open_escape_sequences): Splitter()
{
    _delimiter = std::string(1, delimiter);
    _authorized_open_escape_sequences = authorized_open_escape_sequences;
}

Splitter::Splitter(std::string_view delimiter, std::string_view authorized_open_escape_sequences): Splitter()
{
    _delimiter = delimiter;
    _authorized_open_escape_sequences = authorized_open_escape_sequences;
}

Splitter::Splitter(int (*fun)(int), std::string_view authorized_open_escape_sequences): Splitter()
{
    this->set_delimiter_method(fun);
    _authorized_open_escape_sequences = authorized_open_escape_sequences;
}

Splitter::~Splitter()
{
}

bool    Splitter::set_delimiter(std::string_view str)
{
    _delimiter = str;
    return true;
}

bool    Splitter::set_delimiter_spaces()
{
    this->set_delimiter_method(&std::isspace);
    return true;
}

void    Splitter::set_delimiter_method(int (*fun)(int))
{
    _compare_method = fun;
}

bool    Splitter::set_empty_delimitations(bool active)
{
    _empty_delimitations = active;
    return true;
}

bool    Splitter::set_escape_sequences(std::string_view str)
{
    _authorized_open_escape_sequences = str;
    return true;
}

bool    Splitter::set_escape_sequences_all()
{
    _authorized_open_escape_sequences = Str::g_escapes_open;
    return true;
}

int     Splitter::_get_delimiter_offset(const char *s) const
{
    if (_compare_method != nullptr && _compare_method(s[0]) != 0)
        return 1;
    else if (strncmp(s, _delimiter.c_str(), _delimiter.size()) == 0)
        return (int)_delimiter.size();
    return -1;
}

int     Splitter::count_tokens(const char *s) const
{
    int delimiter_count;
    int delimiter_offset;
    int count = 0;
    int i = 0;
    while (i >= 0 && s[i])
    {
        // pass all delimiters
        delimiter_count = 0;
        while (s[i] && (delimiter_offset = this->_get_delimiter_offset(s + i)) > 0)
        {
            // add another room for token if two delimiters or more follows
            if (_empty_delimitations && delimiter_count > 0)
                ++count;
            i = i + delimiter_offset;
            ++delimiter_count;
        }
        // add a room for a token after a delimitation if it is not the end of the string
        // or it ended by a delimiter
        if (s[i] || (_empty_delimitations && delimiter_count > 0))
            ++count;
        while (s[i])
        {
            int closed_at = Str::closing_escape_index(s, i, _authorized_open_escape_sequences.c_str());
            // matched closure
            if (closed_at > 0)
                i = closed_at;
            // never ends - no more tokens
            else if (closed_at == -2)
            {
                i = -1;
                break ;
            }
            // not a closing escape
            else
            {
                // while not a delimiter found
                if (this->_get_delimiter_offset(s + i) > 0)
                    break ;
                ++i;
            }
        }
    }
    return count;
}

std::string_view    Splitter::next_token(const char *s, int *idx) const
{
    int x = *idx;
    int delimiter_offset;
    while (s[x] && (delimiter_offset = this->_get_delimiter_offset(s + x)) > 0)
    {
        // if there is two delimiters following, return empty token
        if (_empty_delimitations && x != *idx)
        {
            *idx = x;
            return "";
        }
        x = x + delimiter_offset;
    }
    int y = x;
    while (s[y])
    {
        int closed_at = Str::closing_escape_index(s, y, _authorized_open_escape_sequences.c_str());
        // matched closure
        if (closed_at > 0)
            y = closed_at;
        // never ends - last token
        else if (closed_at == -2)
        {
            y = strlen(s);
            break ;
        }
        // not a closing escape
        else
        {
            if (this->_get_delimiter_offset(s + y) > 0)
                break ;
            ++y;
        }
    }
    *idx = y;
    return std::string_view(s + x, std::max(0, y - x));
}

std::vector<std::string>    Splitter::split(std::string_view str) const
{
   if (_delimiter.empty() && _compare_method == nullptr)
        return {std::string(str.data(), str.size())};
    const char *s = str.data();
    int tokens = this->count_tokens(s);
    std::vector<std::string> ret;
    ret.resize(tokens);

    int i = 0;
    int j = 0;
    while (tokens-- > 0)
        ret[i++] = this->next_token(s, &j);
    return ret;
}

std::vector<std::string_view>   Splitter::split_view(std::string_view str) const
{
   if (_delimiter.empty() && _compare_method == nullptr)
        return {str};
    const char *s = str.data();
    int tokens = this->count_tokens(s);
    std::vector<std::string_view> ret;
    ret.resize(tokens);

    int i = 0;
    int j = 0;
    while (tokens-- > 0)
        ret[i++] = this->next_token(s, &j);
    return ret;
}

}