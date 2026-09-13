#include <cctype>
#include <stdexcept>
#include <utility>

#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/tools.hpp>

namespace sihd::util
{

SplitterDelimiterMethod SplitterOptions::delimiter_spaces()
{
    return [](int c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
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

Splitter::~Splitter() = default;

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

// returns the size of the delimiter matching at index i in view or -1
int Splitter::_get_delimiter_offset(std::string_view view, size_t i) const
{
    if (_compare_method && _compare_method(view[i]) != 0)
        return 1;
    if (_delimiter.empty() == false && view.substr(i, _delimiter.size()) == _delimiter)
        return (int)_delimiter.size();
    return -1;
}

bool Splitter::_next_token_range(std::string_view view, int *pos, int *begin, int *end) const
{
    const int size = (int)view.size();
    int i = *pos;
    if (i < 0)
        return false;
    const bool check_encloses = _authorized_open_escape_sequences.empty() == false;
    int delimiter_count = 0;
    int delimiter_offset;
    while (i < size && (delimiter_offset = this->_get_delimiter_offset(view, (size_t)i)) > 0)
    {
        // empty token between consecutive delimiters - the delimiter is left for the next call
        if (_empty_delimitations && delimiter_count > 0)
        {
            *begin = i;
            *end = i;
            *pos = i;
            return true;
        }
        i = i + delimiter_offset;
        ++delimiter_count;
    }
    *pos = i;
    // no token after the delimiters unless the string ended by one in empty mode
    if (i >= size && (_empty_delimitations == false || delimiter_count == 0))
        return false;
    *begin = i;
    while (i < size)
    {
        if (check_encloses)
        {
            const int closed_at = str::stopping_enclose_index(view,
                                                              i,
                                                              _authorized_open_escape_sequences.c_str(),
                                                              _escape_char);
            // jump over the enclosure
            if (closed_at > 0)
            {
                i = closed_at;
                continue;
            }
            // never closes - the token takes the rest of the string
            if (closed_at == -2)
            {
                i = size;
                break;
            }
        }
        if (this->_get_delimiter_offset(view, (size_t)i) > 0)
            break;
        ++i;
    }
    *end = i;
    *pos = i;
    return true;
}

std::vector<std::string> Splitter::split(std::string_view view) const
{
    std::vector<std::string> ret;
    for (std::string_view token : this->split_view(view))
        ret.emplace_back(token);
    return ret;
}

std::vector<std::string_view> Splitter::split_view(std::string_view view) const
{
    if (_delimiter.empty() && !_compare_method)
        return {view};

    std::vector<std::string_view> ret;
    int pos = 0;
    int begin;
    int end;
    while (this->_next_token_range(view, &pos, &begin, &end))
        ret.emplace_back(view.substr((size_t)begin, (size_t)(end - begin)));
    return ret;
}

int Splitter::count_tokens(std::string_view view) const
{
    int count = 0;
    int pos = 0;
    int begin;
    int end;
    while (this->_next_token_range(view, &pos, &begin, &end))
        ++count;
    return count;
}

std::string_view Splitter::next_token(std::string_view view, int *idx) const
{
    int begin;
    int end;
    if (this->_next_token_range(view, idx, &begin, &end) == false)
        return "";
    return view.substr((size_t)begin, (size_t)(end - begin));
}

} // namespace sihd::util
