#include <strings.h>

#include <cctype>
#include <cstring>
#include <stdexcept>

#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/tools.hpp>

namespace sihd::util
{

SplitterDelimiterMethod SplitterOptions::delimiter_spaces()
{
    return &isspace;
}

std::string_view SplitterOptions::all_encloses()
{
    return str::encloses_start();
}

Splitter::Splitter(const SplitterOptions & options): _empty_delimitations(false)
{
    if (sihd::util::tools::maximum_one_true(options.delimiter_char != 0,
                                            options.delimiter_str.size() > 0,
                                            options.delimiter_method != nullptr)
        == false)
    {
        throw std::logic_error("Cannot have two delimiter types");
    }

    this->set_escape_char(options.escape_char);

    if (options.delimiter_char != 0)
        this->set_delimiter_char(options.delimiter_char);
    else if (options.delimiter_str.size() > 0)
        this->set_delimiter(options.delimiter_str);
    else if (options.delimiter_method)
        this->set_delimiter_method(options.delimiter_method);

    this->set_empty_delimitations(options.empty_delimitations);
    this->set_open_escape_sequences(options.open_escape_sequences);
}

Splitter::Splitter(int delimiter): Splitter()
{
    this->set_delimiter_char(delimiter);
}

Splitter::Splitter(std::string_view delimiter): Splitter()
{
    this->set_delimiter(delimiter);
}

Splitter::~Splitter() {}

void Splitter::set_delimiter_char(int delimiter)
{
    _delimiter = std::string(1, delimiter);
}

void Splitter::set_delimiter(std::string_view str)
{
    _delimiter = str;
}

void Splitter::set_delimiter_spaces()
{
    this->set_delimiter_method(SplitterOptions::delimiter_spaces());
}

void Splitter::set_delimiter_method(SplitterDelimiterMethod method)
{
    _compare_method = std::move(method);
}

void Splitter::set_escape_char(int escape_char)
{
    _escape_char = escape_char;
}

void Splitter::set_empty_delimitations(bool active)
{
    _empty_delimitations = active;
}

void Splitter::set_open_escape_sequences(std::string_view str)
{
    _authorized_open_escape_sequences = str;
}

void Splitter::set_escape_sequences_all()
{
    _authorized_open_escape_sequences = SplitterOptions::all_encloses();
}

int Splitter::_get_delimiter_offset(const char *s) const
{
    if (_compare_method && _compare_method(s[0]) != 0)
        return 1;
    else if (strncmp(s, _delimiter.c_str(), _delimiter.size()) == 0)
        return (int)_delimiter.size();
    return -1;
}

int Splitter::count_tokens(std::string_view view) const
{
    const char *s = view.data();
    const size_t size = view.size();
    int delimiter_count;
    int delimiter_offset;
    int count = 0;
    int i = 0;
    while (i >= 0 && i < (int)size)
    {
        // pass all delimiters
        delimiter_count = 0;
        while (i < (int)size && (delimiter_offset = this->_get_delimiter_offset(s + i)) > 0)
        {
            // add another room for token if two delimiters or more follows
            if (_empty_delimitations && delimiter_count > 0)
                ++count;
            i = i + delimiter_offset;
            ++delimiter_count;
        }
        // add a room for a token after a delimitation if it is not the end of the string
        // or it ended by a delimiter
        if (i < (int)size || (_empty_delimitations && delimiter_count > 0))
            ++count;
        while (i < (int)size)
        {
            int closed_at = str::stopping_enclose_index(s, i, _authorized_open_escape_sequences.c_str(), _escape_char);
            // matched closure
            if (closed_at > 0)
                i = closed_at;
            // never ends - no more tokens
            else if (closed_at == -2)
            {
                i = -1;
                break;
            }
            // not a closing escape
            else
            {
                // while not a delimiter found
                if (this->_get_delimiter_offset(s + i) > 0)
                    break;
                ++i;
            }
        }
    }
    return count;
}

std::string_view Splitter::next_token(std::string_view view, int *idx) const
{
    const char *s = view.data();
    const size_t size = view.size();
    int x = *idx;
    int delimiter_offset;
    while (x < (int)size && (delimiter_offset = this->_get_delimiter_offset(s + x)) > 0)
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
    while (y < (int)size)
    {
        int closed_at = str::stopping_enclose_index(s, y, _authorized_open_escape_sequences.c_str(), _escape_char);
        // matched closure
        if (closed_at > 0)
            y = closed_at;
        // never ends - last token
        else if (closed_at == -2)
        {
            y = view.size();
            break;
        }
        // not a closing escape
        else
        {
            if (this->_get_delimiter_offset(s + y) > 0)
                break;
            ++y;
        }
    }
    *idx = y;
    return std::string_view(s + x, std::max(0, y - x));
}

std::vector<std::string> Splitter::split(std::string_view view) const
{
    if (_delimiter.empty() && !_compare_method)
        return {std::string(view)};
    int tokens = this->count_tokens(view);
    std::vector<std::string> ret;
    ret.resize(tokens);

    int i = 0;
    int j = 0;
    while (tokens-- > 0)
        ret[i++] = this->next_token(view, &j);
    return ret;
}

std::vector<std::string_view> Splitter::split_view(std::string_view view) const
{
    if (_delimiter.empty() && !_compare_method)
        return {view};
    int tokens = this->count_tokens(view);
    std::vector<std::string_view> ret;
    ret.resize(tokens);

    int i = 0;
    int j = 0;
    while (tokens-- > 0)
        ret[i++] = this->next_token(view, &j);
    return ret;
}

} // namespace sihd::util
