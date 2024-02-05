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
        int escape_char = '\\';
        // choose one of the three delimiter types
        int delimiter_char = 0;
        // choose one of the three delimiter types
        std::string_view delimiter_str = "";
        // choose one of the three delimiter types
        SplitterDelimiterMethod delimiter_method = {};

        std::string_view open_escape_sequences = "";
        bool empty_delimitations = false;

        static SplitterDelimiterMethod delimiter_spaces();
        static std::string_view all_encloses();

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

        void set_escape_char(int escape_char);
        void set_empty_delimitations(bool active);
        void set_delimiter_char(int delimiter);
        void set_delimiter(std::string_view delimiter);
        // uses std::isspace as a delimiter method
        void set_delimiter_spaces();
        void set_open_escape_sequences(std::string_view sequences);
        // uses sihd::util::str::encloses_start to handle escape sequences
        void set_escape_sequences_all();

        std::vector<std::string> split(std::string_view str) const;
        std::vector<std::string_view> split_view(std::string_view str) const;

        // count number of tokens to return
        int count_tokens(std::string_view view) const;
        // return the next token and updates the index
        std::string_view next_token(std::string_view view, int *idx) const;

    protected:

    private:
        // return the size of the delimiter matching the string c or -1
        int _get_delimiter_offset(const char *c) const;

        int _escape_char;
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
