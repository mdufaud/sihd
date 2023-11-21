#ifndef __SIHD_UTIL_SPLITTER_HPP__
#define __SIHD_UTIL_SPLITTER_HPP__

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::util
{

using SplitterDelimiterMethod = std::function<int(int)>;

struct SplitterOptions
{
        int delimiter_char = 0;
        std::string_view delimiter_str = "";
        SplitterDelimiterMethod delimiter_method = {};

        std::string_view open_escape_sequences = "";
        bool empty_delimitations = false;

        static SplitterDelimiterMethod delimiter_spaces();
        static std::string_view all_open_escape_sequences();

        static SplitterOptions none() { return SplitterOptions {}; }
};

class Splitter
{
    public:

        Splitter(const SplitterOptions & options = SplitterOptions::none());
        Splitter(int delimiter);
        Splitter(std::string_view delimiter);
        virtual ~Splitter();

        void set_delimiter_method(SplitterDelimiterMethod method);

        bool set_empty_delimitations(bool active);
        bool set_delimiter_char(int delimiter);
        bool set_delimiter(std::string_view delimiter);
        // uses std::isspace as a delimiter method
        bool set_delimiter_spaces();
        bool set_open_escape_sequences(std::string_view sequences);
        // uses sihd::util::str::all_open_escape_sequences to handle escape sequences
        bool set_escape_sequences_all();

        std::vector<std::string> split(std::string_view str) const;
        std::vector<std::string_view> split_view(std::string_view str) const;

        // count number of tokens to return
        int count_tokens(const char *s) const;
        // return the next token and updates the index
        std::string_view next_token(const char *s, int *idx) const;

    protected:

    private:
        // return the size of the delimiter matching the string c or -1
        int _get_delimiter_offset(const char *c) const;

        // if two delimiters must make an empty token
        bool _empty_delimitations;
        // a delimiter string to split on
        std::string _delimiter;
        // a series of opening sequences like '(' that'll ignore split when between an opening and closing sequence
        std::string _authorized_open_escape_sequences;
        // a method to match delimiter - overrides delimiter
        // int (*_compare_method)(int c);
        SplitterDelimiterMethod _compare_method;
};

} // namespace sihd::util

#endif
